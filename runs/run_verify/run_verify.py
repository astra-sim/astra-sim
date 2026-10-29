# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 Advanced Micro Devices, Inc.

"""
Runs the verifier on an MSCCL++ Python script, or a pre-generated JSON file.
"""

import argparse 
import importlib 
import json 
import os
import pathlib 
import re 
import shutil 
import signal 
import subprocess
import sys
import tempfile
import time 

from typing import Optional, Tuple, List


# Utility methods.

def rmtree(path):
    """
    There's a race condition at some point where `shutil.rmtree` could fail, 
    which may be due to a subprocess still writing to the path by the time we 
    try to remove it. This attempts to fix that issue by re-trying the removal
    once more after a short delay. 
    """

    if os.path.exists(path):
        try:
            shutil.rmtree(path)
        except:
            time.sleep(0.5)  # Half a second (arbitrary, more than enough time). 
            try:
                if os.path.exists(path):
                    shutil.rmtree(path)
            except:
                raise ValueError(f"Error: we had trouble removing \"{path}\".") 


def get_git_root(start_path: str = __file__) -> str: 
    """
    Returns the path of the directory (parent of start_path) which contains 
    .git in it. If no git root exists, raises a FileNotFoundError. 
    """

    cwd = os.path.abspath(start_path)
    while True:
        if os.path.exists(f"{cwd}/.git"):
            return cwd 

        parent = os.path.dirname(cwd)
        if cwd == parent:
            raise FileNotFoundError(f"No .git found from {start_path}")
        
        cwd = parent 


def append_stat(
    stats_path: str, 
    key: str,
    value: str, 
    desc: str
) -> None:
    """
    Appends a statistic to a stats.txt file at the given path.
    """

    assert os.path.exists(stats_path), stats_path 
    
    # Read all stats from the file.
    stats = []
    reg = re.compile(r"([^\s]+)\s+([^\s]+) # (.+)")
    with open(stats_path, "r") as f:
        assert next(f).strip() == "# --- Begin of statistics ---"
        for line in f:
            if line.strip() == "# --- End of statistics ---":
                break

            parts = reg.match(line.strip())
            assert parts, (
                f"Line did not match regex in \"{stats_path}\": {line}"
            )
            stats.append({
                "key": parts[1], 
                "value": parts[2], 
                "desc": parts[3] 
            }) 
        else:
            raise RuntimeError(
                f"Stats file \"{stats_path}\" did not contain ending mark"
            )

    # Append all stats to the same file.
    stats.append({
        "key": key, 
        "value": value, 
        "desc": desc
    }) 
    with open(stats_path, "w") as f: 
        f.write("# --- Begin of statistics\n") 
        for stat in stats:
            f.write(
                stat['key'].ljust(59) + " " + 
                stat['value'].rjust(9) + " # " + 
                stat['desc'] + "\n"
            ) 
        f.write("# --- End of statistics\n") 


# The custom exceptions we have.

class JSONIncorrectException(Exception):
    def __init__(self, msg):
        self.msg = msg 

class TimeoutException(Exception):
    def __init__(self, msg):
        self.msg = msg 

class VerificationFailedException(Exception): 
    def __init__(self, msg):
        self.msg = msg 


def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("msccl_path", default=None, 
        nargs="?")  # Positional argument
    parser.add_argument("--msccl-path", dest="msccl_path_opt", 
        help="The path to either a Python file or JSON file to verify") 
    parser.add_argument("--simdir", type=str, default="simout", 
        help="Path to where to store simulation output (default = simout)")
    parser.add_argument("--sim-name", type=str, default=f"verify-{os.getpid()}",
        help="Name of simulation output traces (default = verify-{pid})") 
    parser.add_argument("--trace", action="store_true", default=False,
        help="Enables trace mode in the simulator") 
    parser.add_argument("--collective-size", type=str, default="256kb", 
        help="The size of the simulated collective (default = 256kb)") 
    parser.add_argument("--memory-mode", type=str, choices=["value", "trace"], 
        default="trace", help="The representation of the functional memory") 
    parser.add_argument("--no-error-exit", action="store_true", default=False, 
        help=("If set, does *not* exit with an error code if simulation was "
              "able to begin, and instead treats it as verification failed"))
    parser.add_argument("--keep", action="store_true", default=False, 
        help="If set, does not remove the simout directory") 
    parser.add_argument("--verbose", action="store_true", default=False,  
        help="Prints debug information")
    args, forward = parser.parse_known_args() 
    args.forward_args = forward 

    # Resolve the positional/keyword path argument into a single argument.
    if (args.msccl_path is None) == (args.msccl_path_opt is None):
        parser.error((
            "MSCCL++ path must be specified either positionally or with "
            "--msccl-path"
        ))
    args.path = (
        args.msccl_path if args.msccl_path is not None 
        else args.msccl_path_opt
    ) 
        
    # Convert all paths to absolute paths.
    args.path = os.path.abspath(args.path) 
    args.simdir = os.path.abspath(args.simdir) 

    return args 


def get_json(
    path: str, 
    build_args: List[str], 
    verbose: bool
) -> Tuple[str, bool]:
    """
    Returns the path to a JSON file. If the provided path is already JSON, 
    returns it. If it's a Python file, runs it to produce the new JSON file 
    (after adding "build_args" to this Python command). Also returns whether 
    this path should be deleted in this program. 
    """

    def check(json_path):
        """
        Confirms the JSON file is actually JSON. 
        """

        # The temporary file should be valid JSON.
        try:
            with open(json_path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except json.JSONDecodeError:
            with open(json_path, "r", encoding="utf-8") as f:
                content = f.read() 
            os.remove(json_path)
    
            raise JSONIncorrectException((f"Error: we attempted to create a "
                f"JSON file from the Python script \"{json_path}\", though the "
                f"resulting file at \"{json_path}\" is not valid JSON. "
                f"Deleting the file and outputting the content:\n\n{content}"))
    
    if not os.path.exists(path):
        raise FileNotFoundError(f"Error: path \"{path}\" does not exist.") 

    if path.endswith(".json"):
        check(path) 
        return path, False

    if not path.endswith(".py"):
        raise JSONIncorrectException((f"Error: only Python \".py\" and json "
            "\".json\" files can be provided, received \"{path}\".")) 
    
    # Do we have the ability to run MSCCL++? 
    try:
        module = importlib.import_module("mscclpp")
    except ImportError:
        raise Exception(
            "Error: the MSCCL++ library could not be found. Activate the "
            "MSCCL++ Python environment before running this script."
        )

    # Does the gen_functional_memory command exist?
    if shutil.which("gen_functional_memory") is not None:
        cmd = ["gen_functional_memory"] 
    else:
        # We may be able to find it still by going to it directly. First, get 
        # the directory of the installed project.
        if hasattr(module, "__file__") and module.__file__:
            module_path = pathlib.Path(module.__file__).resolve().parent
        elif hasattr(module, "__path__"):
            module_path = pathlib.Path(next(iter(module.__path__))).resolve() 
        else:
            raise Exception(
                "Error: could not find the \"gen_functional_memory\" file. We "
                "attempted to infer this, but we could not automatically find "
                "the MSCCL++ directory path either. Solve this by installing "
                "the \"gen_functional_memory\" file and adding it to your PATH."
            )
        module_path = pathlib.Path(module_path) 

        # Get the root of the git repository of the installed mscclpp project. 
        for parent in [module_path] + list(module_path.parents):
            if (parent / ".git").exists():
                break
        else: 
            raise Exception(
                "Error: could not find the \"gen_functional_memory\" file. We "
                "attempted to infer this, but we could not automatically find "
                "the MSCCL++ git root directory either. Solve this by "
                "installing the \"gen_functional_memory\" file and adding it "
                "to your PATH."
            )

        cmd_path = f"{parent}/python/gen_functional_memory"
        if not os.path.exists(cmd_path):
            raise Exception(
                "Error: could not find the \"gen_functional_memory\" file. We "
                "attempted to infer this, but we could not automatically find "
                f"the path of it at {cmd_path}. Solve this by installing "
                "the \"gen_functional_memory\" file and adding it to your PATH."
            )
        
        cmd = [sys.executable, cmd_path]

    tmp = tempfile.NamedTemporaryFile(
        delete=False, 
        suffix=".json", 
        mode="w", 
        encoding="utf-8"
    )    
    cmd += [path] + build_args
    
    if verbose:
        print(f"Running command: {' '.join(cmd)}") 

    result = subprocess.run(cmd, stdout=tmp, stderr=tmp, text=True)
    tmp.close() 
    
    check(tmp.name)  # Confirm it's JSON. 
    return tmp.name, True


def verify(
    simdir: str, 
    simname: str, 
    json: str, 
    collective_size: str, 
    memory_mode: str, 
    trace: bool, 
    keep: bool, 
    verbose: bool
) -> bool:
    """
    Runs the program and returns a status of True if the program succeeded or 
    False otherwise. Also prints the contents of stderr to the screen, in the 
    case of a failure occurring.
    """

    # Get the absolute path to the JSON file. 
    assert os.path.exists(json), f"Error: \"{json}\" does not exist." 
    json = os.path.abspath(json) 

    # Get the path to the simple script. 
    root = get_git_root() 
    script = f"{root}/runs/run_simple/run_simple.py" 
    assert os.path.exists(script), script  
    
    rundir = os.path.abspath(f"{simdir}/{simname}") 
    cmd = [
        sys.executable, script, 
        "--msccl-plan", json, 
        "-d", simdir, 
        "--collective-size", collective_size, 
        "--target-simple-model", "congestion_unaware", 
        "--sim-name", simname,
        "--functional-simulation", 
        "--memory-mode", memory_mode
    ]
    if trace:
        cmd += [
            "--trace", 
            "--trace-events", "functional"
        ]

    if verbose:
        print(f"Running command: {' '.join(cmd)}") 
    
    # Time limit in seconds. Functional simulation really shouldn't take more 
    # than 5 seconds to run. 
    time_limit = 5
    
    try:
        try:
            proc = subprocess.run(
                cmd, 
                capture_output=True, 
                text=True, 
                timeout=time_limit, 
                cwd=(os.path.dirname(os.path.abspath(script)))
            )
        except subprocess.TimeoutExpired as ex: 
            raise TimeoutException((
                f"Error: the simulator took more than {time_limit} seconds to "
                f"run. This likely indicates a problem in the simulator."
            )) from ex
    
        if proc.returncode != 0:
            # If the command failed, it was either due to (1) the launcher 
            # failing, or (2) there being some assertion or exception in the 
            # simulator. If it's (2), then ignore this error and handle it 
            # later.
            err_fname = f"{rundir}/{simname}.err"
            if (
                os.path.exists(err_fname) and 
                os.path.getsize(err_fname) > 0
            ): 
                # Case (2), do nothing.
                pass
            else:
                # Case (1), launcher error.
                raise Exception((
                    f"Error in launching command \"{' '.join(cmd)}\":\n\n"
                    f"{proc.stdout}\n{proc.stderr}"
                ))
    
        fnames = [
            f"{rundir}", 
            f"{rundir}/{simname}.out", 
            f"{rundir}/{simname}.err"
        ]
        if not all(os.path.exists(fname) for fname in fnames):
            raise FileNotFoundError(
                f"Error: the simulator failed to produce all files in {fnames}"
            )
    
        with open(f"{rundir}/{simname}.out", "r") as f: 
            output = f.read() 
        with open(f"{rundir}/{simname}.err", "r") as f: 
            error = f.read()
    
        # The output should say either "Verifier passed!" or "Verifier failed!".
        if "Verifier failed!" in output or len(error) > 0:
            print(
                "Error: the collective output did not match our expectations."
            )
            print()
            print("Verifier output:")
            for idx, line in enumerate(error.strip().split("\n"), 1):
                print(f"{idx}.) {line}")
            print()
            return False
        elif "Verifier passed!" in output:
            return True
        else:
            raise Exception(f"Simulator error: could not find verifier status "
                "message in output.") 
        
        return True
    
    finally:
        if not trace and not keep and os.path.exists(rundir):
            if verbose:
                print(f"Removing \"{rundir}\"") 
            rmtree(rundir)  


def main(
    path: str, 
    forward_args: List[str], 
    simdir: str, 
    simname: str, 
    collective_size: str, 
    memory_mode: str, 
    trace: bool, 
    keep: bool, 
    no_error_exit: bool, 
    verbose: bool
): 
    """
    Performs the entire verification routine. Prints messages to stdout. If an 
    error occurs, raises an exception directly. Otherwise, returns nothing.

    We have several custom exceptions:
      * JSONIncorrectException, raised if the JSON file provided is not valid.
      * TimeoutException, representing a verification command which took too 
        long to execute.
      * VerificationFailedException, raised if the simulator could successfully 
        run, but the verifier indicated the program was incorrect.
    """

    json, remove = get_json(
        path, 
        forward_args,
        verbose
    )
    
    rundir = f"{simdir}/{simname}" 
    success = False 
    try:
        status = verify(
            simdir, 
            simname, 
            json, 
            collective_size, 
            memory_mode, 
            trace, 
            keep, 
            verbose
        )
        if status:
            print("Verifier passed!")
            success = True
        else:
            print("Verifier failed!")
            success = False 
    except TimeoutException: 
        if no_error_exit: 
            # Treat this as a simulation error and consider it a fail.
            print("Verifier failed! (Simulation timeout)") 
            success = False
    finally:
        if remove and not keep:
            if verbose:
                print(f"Removing \"{json}\"")
            os.remove(json)

    # Write simulation success/failure to output file. 
    if keep:
        stats_path = f"{rundir}/stats.txt" 
        if os.path.exists(stats_path):
            append_stat(
                stats_path=stats_path, 
                key="verifier_passed", 
                value=("0" if success else "1"), 
                desc=(
                    "Whether or not the verifier passed (success = 0, fail = 1)"
                )
            )

    if not success and not no_error_exit:
        # If "no_error_exit" is set, always return a success code of 0 if the 
        # simulation was able to launch at all. This allows batch scripts to 
        # flag if some error occurred in JSON generation while allowing them to
        # launch all jobs in the case of regular validation.
        raise VerificationFailedException("Verifier failed!") 
    
    return success


if __name__ == "__main__":
    args = get_args()
    result = main(
        args.path, 
        args.forward_args, 
        args.simdir, 
        args.sim_name, 
        args.collective_size, 
        args.memory_mode,
        args.trace, 
        args.keep, 
        args.no_error_exit, 
        args.verbose
    )

    if not result and not args.no_error_exit:
        sys.exit(1) 
