* **num-npus**: (int) Total number of NPUs we are simulating
* **num-packages**: (int) Total number of packages (each could contain one or multiple NPUs)
* **package-rows**: (int) Number of package rows. it defines the vertical dimension size
* **topology**: (NV_Switch/Torus3D) Determines the physical topology
* **local-rings**: (int) Determines the number of rings in the local (intra-package) dimension
* **vertical-rings**: (int) Determines the number of rings in the vertical (inter-package) dimension (applicable only on the Torus3D topology)
* **horizontal-rings**: (int) Determines the number of rings in the horizontal (inter-package) dimension (applicable only on the Torus3D topology)
* **links-per-tile**: (int) Determines the number of links for the alltoall (inter-package) dimesnion (applicable only on the NV_Switch topology)
* **flit-width**: (int) The width of flits in bits
* **local-packet-size**: (int) The size of intra-pcakge packets in bytes
* **package-packet-size**: (int) The size of inter-pcakge packets in bytes
* **tile-link-width**: (int) The width of intra-pcakge links in  bits
* **package-link-width**: (int) The width of inter-pcakge links in  bits
* **vcs-per-vnet**: (int)  Number of VCs per each Vnet
* **routing-algorithm**: (Ring_XY/AllToAll) Routing algorithm 
* **router-latency**: (int) Delay at each router
* **local-link-latency**: (int) delay of intra-package links in cycles
* **package-link-latency**: (int) delay of inter-package links in cycles
* **buffers-per-vc**: (int) Buffer size per each VS in terms of number of flits
* **local-link-efficiency**: (float) The ratio of (data/header+data) for intra-package packets
* **package-link-efficiency**: (float) The ratio of (data/header+data) for inter-package packets
