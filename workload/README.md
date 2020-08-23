This layer sits on top of system layer and implements the different training loops and parallelization strategies.
It talks to the system layer using collective primitives provided by system layer.



Some eamples of workload input are provided in `workload_inputs`

A script is also provided to generate the workload scripts in folder `workload_generator`. It currently only supports GEMM type layers and the same parallelization strategy specified by users is applied to all the layer.