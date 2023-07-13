# ASTRA-sim 2.0
ASTRA-sim is a distributed machine learning system simulator, developed as a joint collaboration between Georgia Tech, Meta, and Intel.
The previous version, ASTRA-sim 1.0, is available in the `ASTRA-sim-1.0` branch.

Here is a concise visual summary of our simulator:
![alt text](https://github.com/astra-sim/astra-sim/blob/master/docs/images/astrasim_overview_codesign.png)

For a comprehensive understanding of the tool, and to gain insights into its capabilities, please refer to our paper:

William Won, Taekyung Heo, Saeed Rashidi, Srinivas Sridharan, Sudarshan Srinivasan, and Tushar Krishna, "ASTRA-sim2.0: Modeling Hierarchical Networks and Disaggregated Systems for Large-model Training at Scale". In Proceedings of the IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS), April 2023. [[pdf]](https://arxiv.org/pdf/2303.14006.pdf)

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
ASTRA-sim can be built either in your local environment or within a Docker container.
The following steps will guide you through both methods.
### 1. Build ASTRA-sim Locally
To build ASTRA-sim without using Docker, you first need to install the necessary packages.
This can be done using the following commands:
```bash
$ apt-get -y update
$ apt-get -y install\
    gcc g++ make cmake\
    libboost-dev libboost-program-options-dev\
    libprotobuf-dev protobuf-compiler\
    python3 python3-pip git
$ pip3 install protobuf==3.6.1 pydot
```

Once the packages are installed, you will need to clone this repository onto your local machine using the following command:
```bash
$ git clone --recurse-submodules git@github.com:astra-sim/astra-sim.git
```

Then, based on your target network backend, execute the corresponding build script:
```bash
# For the analytical network backend
$ ./build/astra_analytical/build.sh -c
```

### 2. Build ASTRA-sim in a Docker Image
Alternatively, you can build ASTRA-sim within a Docker container.
Start by cloning this repository to your local machine using the same command as above:
```bash
$ git clone --recurse-submodules git@github.com:astra-sim/astra-sim.git
```

Next, create a Docker image using the following command:
```bash
$ docker build -t astra-sim .
```

Once the Docker image is created, you can run it with this command:
```bash
$ docker run -it astra-sim
```

Finally, similar to the local build process, depending on your target network backend, you should run the corresponding build script:
```bash
# For the analytical network backend
$ ./build/astra_analytical/build.sh -c
```

## Running ASTRA-sim
Once ASTRA-sim is built, conduct experiments by passing the required configurations.
You might need to provide additional configurations based on the network backend.
The following configurations are mandatory:
* --workload-configuration: Path prefix to the execution trace. The naming rule for execution traces follows the format `{path prefix}.{npu_id}.eg`. This argument provides the path prefix.
* --system-configuration: Path to the system configuration. Example system configurations can be found at `inputs/system/`.
* --network-configuration: Path to the network configuration Example network configurations can be found at `inputs/network/`.

Execution traces can be created using Chakra tools. You have the option of using either [the execution trace generator (et_generator)](https://github.com/chakra-et/chakra#execution-trace-generator-et_generator)
or [the execution trace converter (et_converter)](https://github.com/chakra-et/chakra#execution-trace-converter-et_converter).
The et_generator can be used to define and generate any execution traces, functioning as a test case generator. Meanwhile, the et_converter is a trace schema conversion tool, supporting PyTorch and FlexFlow execution traces, as well as ASTRA-sim 1.0 input files.

### Using the Execution Trace Generator
You can generate execution traces with et_generator with the following commands.
```bash
$ cd extern/graph_frontend/chakra/et_generator
$ cmake CMakeLists.txt && make -j$(nproc)
$ ./et_generator --num_npus 64 --num_dims 1
```

To run one of the example traces (`twoCompNodesDependent`), execute the following command.
```bash
$ ./build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra \
  --workload-configuration=./extern/graph_frontend/chakra/et_generator/twoCompNodesDependent \
  --system-configuration=./inputs/system/sample_fully_connected_sys.txt \
  --network-configuration=./inputs/network/analytical/fully_connected.json
```

Upon completion, ASTRA-sim will display the number of cycles it took to run the simulation.
```bash
sys[0] finished, 10 cycles
sys[1] finished, 10 cycles
...
sys[62] finished, 10 cycles
sys[63] finished, 10 cycles
```

### Using the Execution Trace Converter
You can convert ASTRA-sim 1.0 text input files into Chakra traces with the following commands.
```bash
$ cd extern/graph_frontend/chakra/
$ python3 setup.py install
$ python3 -m et_converter.et_converter\
    --input_type Text\
    --input_filename ../../../inputs/workload/ASTRA-sim-1.0/Resnet50_DataParallel.txt\
    --output_filename ../../../inputs/workload/ASTRA-sim-2.0/Resnet50_DataParallel\
    --num_npus 64\
    --num_dims 1\
    --num_passes 1
```

Run the following command.
```bash
$ ./build/astra_analytical/build/AnalyticalAstra/bin/AnalyticalAstra \
  --workload-configuration=./inputs/workload/ASTRA-sim-2.0/Resnet50_DataParallel \
  --system-configuration=./inputs/system/sample_fully_connected_sys.txt \
  --network-configuration=./inputs/network/analytical/fully_connected.json
```

Upon completion, ASTRA-sim will display the number of cycles it took to run the simulation.
```bash
sys[62] finished, 187442108 cycles
sys[61] finished, 187442108 cycles
...
sys[0] finished, 187442108 cycles
sys[63] finished, 187442108 cycles
```

## Features Under Active Development
We are constantly working to improve ASTRA-sim and expand its capabilities. Here are some of the features that are currently under active development:

* Memory API
* Congestion-aware Analytical Network Backend
* NS3 Network Backend
* Garnet Network Backend
* Detailed Statistics Report (Network Utilization)
  
Please note that these features are under active development and, while we aim to have them available as soon as possible, the completion timeline can vary. Check back regularly for updates on the progress of these and other features. We appreciate your interest and support in ASTRA-sim!


## Contact Us
This project is a collaboration of dedicated professionals. Each core developer and contributor plays a unique role in the project. For any inquiries or questions, feel free to reach out to the corresponding developer based on their expertise. 
| Developer | Organization | Responsibility | Contact |
|-----------|--------------|----------------|---------|
| Saeed Rashidi | Hewlett Packard Labs | ASTRA-sim 1.0, system layer, communicator groups, in-switch collective communication | rashidi1saeid@gmail.com |
| William Won | Georgia Tech | Network layer | william.won@gatech.edu |
| Taekyung Heo | Georgia Tech | Chakra, workload layer, graph execution engine, memory API | taekyung@gatech.edu |
| Srinivas Sridharan | Meta | Chakra, General inquiries | ssrinivas@fb.com |
| Tushar Krishna | Georgia Tech | General inquiries | tushar@ece.gatech.edu |

## Additional Contributors
* Jiayi Huang (University of California, Santa Barbara)
* Apurve Chawde (Georgia Tech)
* Santosh Kumar Elangoven (Georgia Tech)
* Greg Steinbrecher (Meta)
* Changhai Man (Georgia Tech)
