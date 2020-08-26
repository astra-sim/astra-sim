*  **scheduling-policy**: (LIFO/FIFO) 
	* The order we proritize collectives according based on their time of arrival.
        LIFO means that most recently created collectives have higher priority. While
	FIFO is the reverse.
*   **endpoint-delay **: (int)
	* The time NPU spends processing a message after receiving it in terms of cycles.
*   **packet-routing**: (software/hardware)
	* software means that the ubderlying network only can support neighbor-to-neighbor 
	direct communication and software needs to handle non-neighbor communication.
        hardware means that the network protocol allows non-neighbor direct communication.
*  **injection-policy **: (normal/aggressive)
	* injection policy refers to the degree we allow concurrent chunks/messages being in-flight inside the 
        network.
*  **preferred-dataset-splits**: (int)
	* The number of chunks we divide each collective into.
*  **boost-mode**: (0/1)
	* 0 means all nodes are simulated. 1 means that only the nodes
	directly engaged with node 0 (node 0 collects all of the stats)
	are simulated. It is a technique to reduce the simulation time
	by trading some (minor) accuracy.
* **collective-implementation**: (allToAll/doubleBinaryTree/hierarchicalRing)
	* The collective communication algorithm we execute. In the
	current version,  allToAll and doubleBinary tree are ONLY
	supported on NV-Switch physical topology while
	hierarchicalRing is supported ONLY on Torus physical topology. 
* **collective-optimization**: (baseline/localBWAware)
	* baseline issues allreduce across all dimensions to handle
	allreduce of single chunk. While localBWAware issues a 
	reduce-scatter on  local dimension first (to beak data size)
	followed by allreduce on other dimensions, followed by final allgather
	on the local dimension.

*NOTE: The default clock cycle period is 1ns (1 Ghz feq). This value is defined inside Sys.hh.
One can change it to any number. It will be a configurable command line parameter in the later
versions.*