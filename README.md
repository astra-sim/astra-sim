# ASTRA-Sim #

### What is this repository for? ###
This is the ASTRA-sim distributed Deep Learning Training simulator, developed in collaboration between Georgia Tech, Facebook and Intel.

An overview is presented here:
![alt text](https://github.com/astra-sim/astra-sim/blob/master/docs/images/astrasim_overview_codesign.png)

The full description of the tool and its strength can be found in the paper below:

Saeed Rashidi, Srinivas Sridharan, Sudarshan Srinivasan, and Tushar Krishna,
"ASTRA-SIM: Enabling SW/HW Co-Design Exploration for Distributed DL Training Platforms"
*In Proc of the IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS), Apr 2020*
[[pdf]](https://sites.gatech.edu/ece-synergy/files/2020/08/astrasim_ispass2020.pdf)[[slides]](https://cpb-us-w2.wpmucdn.com/sites.gatech.edu/dist/c/332/files/2020/08/ISPASS2020-ASTRA-SIM_talk.pdf)[[video]](https://www.youtube.com/watch?v=S-HE9yBv8_I&list=PLHJB2bhmgB7crXM7wBKIDi7OEa0UTZtrR&index=10)

ASTRA-SIM tutorials can be found here: https://astra-sim.github.io/

**Bibtex**

    @inproceedings{astrasim,
        author       = {Saeed Rashidi and
                       Srinivas Sridharan and
                       Sudarshan Srinivasan and
                       Tushar Krishna},
        title        = {{ASTRA-SIM: Enabling SW/HW Co-Design Exploration for Distributed DL Training Platforms}},
        booktitle     = {{IEEE} International Symposium on Performance Analysis of Systems
                        and Software, {ISPASS} 2020, Boston, MA, USA, August 22-26, 2020},
      publisher     = {{IEEE}},
      year          = {2020},
    }


### Setup Instructions ###

```bash
# Clone the repository
$ git clone https://github.com/astra-sim/astra-sim.git

# cloning the submodules
$ cd astra-sim
$ git submodule init
$ git submodule update
```

#### Instructions for compiling & running Garnet2.0 as the network simulator
1. Run `./build/astra_garnet/build.sh -c` to compile and integrate astra-sim with gem5 (`-l` flag will clean the compilation). This will create a binary file where garnet is integrated with astra-sim. The analytical backend is hosted at  https://github.com/georgia-tech-synergy-lab/gem5_astra .
2. Run an example inside the `examples/` directory with garnet as a backend. Example: `examples/run_allreduce.sh -n garnet`. This command will run a single all-reduce collective on a Torus topology. 
3. The results of example script runs will be dumped inside `examples/results/` path.

#### Instructions for compiling & running analytical backend as the network simulator
1. Run `./build/astra_analytical/build.sh -c` to compile and integrate astra-sim with analytical backend (`-l` flag will clean the compilation). This will create a binary file where analytical backend is integrated with astra-sim. Please refer to [this page](https://github.com/astra-sim/astra-sim/tree/master/build/astra_analytical) for more details about compilation. The analytical backend is hosted at https://github.com/astra-sim/analytical .
2. Run an example inside the `examples/` directory with the analytical model as a backend. Example: `examples/run_allreduce.sh -n analytical`. This command will run a single all-reduce collective on a Torus topology. 
3. The results of example script runs will be dumped inside `examples/results/` path. 

#### Instructions for compiling & running NS3 as the network simulator
1. Check-out to the **ns3_interface_tmp2** branch by running `git checkout ns3_interface_tmp2` .
2. Update the submodules by typing `git submodule init & git submodule update`. The ns3 backend is hosted at https://github.com/DartingMelody/ns3-interface.
3. Go to the ns3 directory: `cd extern/network_backend/ns3-interface/simulation/` and run `./waf configure` (only required once)
4. Now go back to the root directory of ASTRA-SIM and run "./build/astra_ns3/build.sh -c". This will first copy the ASTRA-SIM/ns3 glue code (stored in the `astra-sim/network_frontend/ns3` directory and contains the main file), and then build and run the ns3 main file. Please refer to the `astra-sim/network_frontend/ns3` for more information about the glue code.
5. The stat files are written into `extern/network_backend/ns3-interface/simulation/scratch/results/` address. Note that this version supports multi-workload cases, meaning that for example, with the total of P NPUs, NPUs 0 : N can be allocated for one workload, and NPUs N+1 : P-1 for another workload. If multiple workloads are used, then each worklaod produces its own set of stat files. The stat file names are appended by number, showing the index of first NPU assigned for that workload.

NOTE: The on-screen reported delays (no matter what backend is used) after the end of simulation are in cycles while the delays inside the csv files are in terms of microseconds.

#### ASTRA-SIM Binary Command Line Options
When running the binary file (no matter what backend is used), the following options may be passed to the binary file (see example scripts):

**--network-configuration (required):** The network input file dir.

**--system-configuration  (required):** The system input file dir.

**--workload-configuration (required):** The workload input file dir.

**--path (required):** The path to dump the results.

**--run-name  (required):** Name of the current run.

**--num-passes  (required):** Number of training passes to simulate.

**--total-stat-rows (required):** Total number of runs that want to write to the same csv file (please see run_multi.sh inside the "examples/"" directory). This is useful when multiple runs want to write to the same csv file. This value should be 1 if only 1 run is executed. 

**--stat-row  (required):** The position of the run to write its stats into the csv stat files (please see run_multi.sh inside the "examples/"" directory). This is useful when multiple runs want to write to the same csv file. This value should be 0 if only 1 run is executed.

**--compute-scale (optional):** Scales the all compute times (reported in the workload input file) by this scale. Tge default value is 1.

**--comm-scale  (optional):** Scales the all communication sizes (reported in the workload input file) by this scale. Tge default value is 1.

NOTE: The garnet+astra-sim binary also allows all of the network input options be overridden by the command line options. 

### Input Files to ASTRA-sim ###

* Workload: `inputs/workload/`
   * see `inputs/workload/README.md`
   * see `scripts/workload_generator/README.md` for instruction on how to use an automated script to generate workload input files.
* System: `inputs/system/`
   * see `inputs/system/README.md`
* Network: 
    * `inputs/network/garnet` (for garnet backend inputs)
      * see inputs/network/garnet/README.md`
    * `inputs/network/analytical` (for analytical backend inputs)
      * see `inputs/network/analytical/README.md`
    

### Contact ###
Please email Saeed Rashidi (saeed.rashidi@gatech.edu) or Srinivas Sridharan (ssrinivas@fb.com) or Tushar Krishna (tushar@ece.gatech.edu) if you have any questions.

### Core Developers ###
* Saeed Rashidi (Georgia Tech)
* Srinivas Sridharan (Facebook)

### Additional Contributors ###
* Jiayi Huang (University of California, Santa Barbara)
* Apurve Chawde (Georgia Tech)
* Santosh Kumar Elangoven (Georgia Tech)
* William Won (Georgia Tech)
* Tushar Krishna (Georgia Tech)
* Greg Steinbrecher (Facebook)
