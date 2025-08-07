This folder contains example workload (Chakra), system, and network input files.

### Workload
- `microbenchmarks`: Contains simple Chakra ET files for 1MB collective communication microbenchmarks, for 4, 8, and 16 NPUs.
    - All-Reduce
    - All-Gather
    - Reduce-Scatter
    - All-to-All

### System
- `native_collectives`: ASTRA-sim system layer config files that's using ASTRA-sim's native collective algorithm implementations.
- `custom_collectives`: System layer config file using custom collective implementations via ASTRA-sim's CollectiveAPI.

### Network
- `analytical`: Analytical network input files.
- `ns3`: ns-3 network backend input files.
- `htsim`: HTSim network backend input files.

### Run Scripts
Includes scripts to run sample ASTRA-sim simulations. Please run existing `.sh` files to execute example ASTRA-sim runs.
- `analytical`: Example scripts to run ASTRA-sim with analytical network backends. This directory includes two variations:
    - `congestion_unaware`
    - `congestion_aware`
- `ns3`: Example scripts to run ASTRA-sim with ns-3 network backend.
- `htsim`: Example script to run ASTRA-sim with HTsim network backend.
