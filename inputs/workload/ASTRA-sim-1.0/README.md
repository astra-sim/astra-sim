The format of workload input is as follows (NOTE that all communication sizes are in bytes and compute times are in terms of cycles):

* **first line**: (DATA/HYBRID_TRANSFORMER/HYBRID_DLRM) 
	* The training loop parallelization type. Each new training loop type sould be implemented inside thw Workload.cc file.
	DATA is the pure data-parallel approach. HYBRID_TRANSFORMER is hybrid-parallel tuned for Transformer DNN network. 	  HYBRID_DLRM is hybrid-parallel tuned for DLRM DNN network.

* **second line**: (int)
	* shows the number of DNN layers

* **subsequent lines**: Each subsequent line describes a layer. The format of layer description  is as follows:
	* {(string: **layer name**) (int: **reserved variable**)
	(int: **forward pass compute time**) (ALLREDUCE/ALLGATHER/ALLTOALL: **forward pass communication type**) (int: **forward pass communication size**)
	(int: **input grad compute time**) (ALLREDUCE/ALLGATHER/ALLTOALL: **input grad communication type**) (int: **input grad communication size**)
	(int: **weight grad compute time**) (ALLREDUCE/ALLGATHER/ALLTOALL: **weight grad communication type**) (int: **weight grad communication size**) 
	(**delay per entire weight/input/output update after the collective is finished**)} 

*NOTE: all parameters inside the bracket are defined in a single line for each layer of the DNN network.* 
	 
