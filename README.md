# ASTRA-sim 2.0
ASTRA-sim is a distributed machine learning system simulator, developed as a joint collaboration between Georgia Tech, Meta, and Intel.
The previous version, ASTRA-sim 1.0, is available in the `ASTRA-sim-1.0` branch.

Here is a concise visual summary of our simulator:
![alt text](https://github.com/astra-sim/astra-sim/blob/master/docs/images/astrasim_overview_codesign.png)

For a comprehensive understanding of the tool, and to gain insights into its capabilities, please refer to our paper:

William Won, Taekyung Heo, Saeed Rashidi, Srinivas Sridharan, Sudarshan Srinivasan, and Tushar Krishna, "ASTRA-sim2.0: Modeling Hierarchical Networks and Disaggregated Systems for Large-model Training at Scale". In Proceedings of the IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS), April 2023.

For tutorials on how to use ASTRA-SIM, please visit our [tutorial page](https://astra-sim.github.io/).

## Citation
If you use ASTRA-sim in your research, please cite our paper:

```
@INPROCEEDINGS{10158106,
    author={Won, William and Heo, Taekyung and Rashidi, Saeed and Sridharan, Srinivas and Srinivasan, Sudarshan and Krishna, Tushar},
    booktitle={2023 IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS)},
    title={ASTRA-sim2.0: Modeling Hierarchical Networks and Disaggregated Systems for Large-model Training at Scale},
    year={2023},
    volume={},
    number={},
    pages={283-294},
    doi={10.1109/ISPASS57527.2023.00035}}
```

## Build Instructions
First, clone this repository to your local machine by running:
```bash
$ git clone --recurse-submodules git@github.com:astra-sim/astra-sim.git
```

Depending on your target network backend, run the appropriate build script:
```bash
# For the analytical network backend
$ ./build/astra_analytical/build.sh -c

# For the garnet network backend (currently unsupported)
$ ./build/astra_garnet/build.sh -c

# For the NS3 network backend (currently unsupported)
$ ./build/astra_ns3/build.sh -c
```

## Running ASTRA-sim
Once ASTRA-sim is built, conduct experiments by passing the required configurations.
You might need to provide additional configurations based on the network backend.
The following configurations are mandatory:
* --workload-configuration: Path prefix to the execution trace.
* --system-configuration: Path to the system configuration. Example system configurations can be found at `inputs/system/`.
* --network-configuration: Path to the network configuration Example network configurations can be found at `inputs/network/`.

Execution traces can be created using Chakra tools. You have the option of using either [the execution trace generator (et_generator)](https://github.com/chakra-et/chakra#execution-trace-generator-et_generator)
or [the execution trace converter (et_converter)](https://github.com/chakra-et/chakra#execution-trace-generator-et_generator).
The et_generator can be used to define and generate any execution traces, functioning as a test case generator. Meanwhile, the et_converter is a trace schema conversion tool, supporting PyTorch and FlexFlow execution traces, as well as ASTRA-sim 1.0 input files.

You can generate execution traces with et_generator with the following commands.
```bash
$ cd extern/graph_frontend/chakra/et_generator
$ cmake CMakeLists.txt && make -j$(nproc)
$ ./et_generator
```

To run one of the example traces (`twoCompNodesDependent`), execute the following command
```bash
$ ./build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra \
  --workload-configuration=./extern/graph_frontend/chakra/et_generator/twoCompNodesDependent \
  --system-configuration=./inputs/system/sample_fully_connected_sys.txt \
  --network-configuration=./inputs/network/analytical/fully_connected.json
```

Upon completion, ASTRA-sim will display the number of cycles it took to run the simulation.

## Contact Us
For any inquiries or questions, please feel free to reach out to:
* Saeed Rashidi (rashidi1saeid@gmail.com)
* Srinivas Sridharan (ssrinivas@fb.com)
* Tushar Krishna (tushar@ece.gatech.edu)

## Core Developers
* Saeed Rashidi (Hewlett Packard Labs)
* Srinivas Sridharan (Meta)

## Additional Contributors
* Jiayi Huang (University of California, Santa Barbara)
* Apurve Chawde (Georgia Tech)
* Santosh Kumar Elangoven (Georgia Tech)
* William Won (Georgia Tech)
* Tushar Krishna (Georgia Tech)
* Greg Steinbrecher (Facebook)
* Taekyung Heo (Georgia Tech)
* Changhai Man (Georgia Tech)
