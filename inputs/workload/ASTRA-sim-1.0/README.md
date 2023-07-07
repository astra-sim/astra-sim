The structure of the workload input adheres to the following format. Please note that all communication sizes are measured in bytes and compute times are denoted in cycles:

* **First Line**: (DATA/HYBRID_TRANSFORMER/HYBRID_DLRM)
  * This line specifies the type of training loop parallelization. DATA refers to a purely data-parallel approach, HYBRID_TRANSFORMER denotes a hybrid-parallel approach tailored for Transformer DNN networks, while HYBRID_DLRM implies a hybrid-parallel approach fine-tuned for DLRM DNN networks.

* **Second Line**: (int)
  * This line indicates the number of layers in the DNN.

* **Subsequent Lines**: Each subsequent line describes a layer. The format of layer description  is as follows:
  * {(string: **layer name**) (int: **reserved variable**) (int: **forward pass compute time**) (ALLREDUCE/ALLGATHER/ALLTOALL: **forward pass communication type**) (int: **forward pass communication size**) (int: **input grad compute time**) (ALLREDUCE/ALLGATHER/ALLTOALL: **input grad communication type**) (int: **input grad communication size**) (int: **weight grad compute time**) (ALLREDUCE/ALLGATHER/ALLTOALL: **weight grad communication type**) (int: **weight grad communication size**) (**delay per entire weight/input/output update after the collective is finished**)}

*NOTE: All parameters within the brackets are defined on a single line for each layer of the DNN network.* 
