This layer sits on top of the network simulator (gem5) and is responsible for executing and managing collective communication algorithms.
It provides different collective primitive APIs to its upper layer that is the workload layer and communicates with its lower layer (gem5)
using the commonapi interface defined in CommonAPI.hh .
