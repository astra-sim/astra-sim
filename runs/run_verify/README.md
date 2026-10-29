# Verifier

This README describes how to use the verifier. At its core, the verifier is a tool which:

* Takes a workload (e.g., an MSCCL++ collective, or some other arbitrary program that AstraSim is capable of running), and
* Takes an ideal memory layout (e.g., generated from the `gen_functional_memory` tool), and
* Determines whether the workload produces that ideal memory layout.

The steps above are currently only implemented for MSCCL++ collectives, but it can be extended to support any arbitrary workload (e.g., GEMMs). The verifier is implemented in a **functional simulation mode** added to AstraSim. This mode is hardware-agnostic and has no concept of compute units or wavefronts, and instead operates on the *chunk* granularity. Functional simulation is very quick relative to timing simulation, and should take less than a second on average. 

## Steps

To run the verifier, do the following steps:

1. Use the `gen_functional_memory` tool to generate a JSON file. For example, `gen_functional_memory /path/to/allgather-ring.py --num_gpus 4 --instances 1 > plan.json`. 
2. Run the command `python3 run_verify.py plan.json` to verify it. If the verifier succeeded, you should see a "Verifier passed!" message in the output. 

The `run_verify.py` script also accepts verifying MSCCL++ Python scripts directly, like `python3 run_verify.py /path/to/allgather-ring.py --num_gpus 4 --instances 1`. All unknown arguments are forwarded to the underlying script. 

To add support for new collective types or to verify programs besides communication collectives, then ensure a "memory" field exists within the outputted JSON file. Override the `gen_functional_memory` script with this functionality. (This script *streams* JSON to reduce memory overhead, but loading the entire JSON file and overwriting it is acceptable, too.)
