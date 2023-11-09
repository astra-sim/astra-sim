# ASTRA-sim 2.0
ASTRA-sim is a distributed machine learning system simulator, developed as a joint collaboration between Georgia Tech, Meta, and Intel.
The previous version, ASTRA-sim 1.0, is available in the `ASTRA-sim-1.0` [branch](https://github.com/astra-sim/astra-sim/tree/ASTRA-sim-1.0).

Here is a concise visual summary of our simulator:
![alt text](https://github.com/astra-sim/astra-sim/blob/master/docs/images/astrasim_overview_codesign.png)

For a comprehensive understanding of the tool, and to gain insights into its capabilities, please refer to our paper:

William Won, Taekyung Heo, Saeed Rashidi, Srinivas Sridharan, Sudarshan Srinivasan, and Tushar Krishna, "ASTRA-sim2.0: Modeling Hierarchical Networks and Disaggregated Systems for Large-model Training at Scale". In Proceedings of the IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS), April 2023. [[pdf]](https://arxiv.org/pdf/2303.14006.pdf) [[slides]](https://astra-sim.github.io/assets/papers/astra-sim2/ASTRA-sim2-ISPASS_2023.pdf) [[video]](https://youtu.be/QGilXx_GEXs?si=CPSZxvGDEm56iOp6)

For tutorials on how to use ASTRA-sim, please visit our [tutorial page](https://astra-sim.github.io/).

## Citation
If you use ASTRA-sim in your research, please cite our paper:
```
@inproceedings{astrasim2,
    author={Won, William and Heo, Taekyung and Rashidi, Saeed and Sridharan, Srinivas and Srinivasan, Sudarshan and Krishna, Tushar},
    booktitle={2023 IEEE International Symposium on Performance Analysis of Systems and Software (ISPASS)},
    title={ASTRA-sim2.0: Modeling Hierarchical Networks and Disaggregated Systems for Large-model Training at Scale},
    year={2023},
    pages={283-294},
    doi={10.1109/ISPASS57527.2023.00035}
}
```

## Build Instructions
ASTRA-sim can be built either (i) in your local environment or (ii) within a Docker container.
The following steps will guide you through both methods.

### 1. Build ASTRA-sim Locally
#### (i) Installing Dependencies
To build ASTRA-sim locally, you first need to install the necessary packages.

- #### Debian-based Linux Distribution
For Debian-based Linux distributions, including Ubuntu, you can install dependencies using the following commands:
```bash
$ sudo apt update
$ sudo apt upgrade
$ sudo apt install \
    gcc g++ make cmake \
    libboost-dev libboost-program-options-dev \
    libprotobuf-dev protobuf-compiler \
    python3 python3-pip git
```

NOTE: For the ns3 backend `python2`, `gcc-5.0` and `g++-5.0` are also required. This is because the ns3 backend is based on an older ns3 version. We recommend using virtual environments to isolate python instances. Even with the ns3 backend, `python3` is still used to create the workload using Chakra.

- #### macOS
For macOS, you can first install required dependencies using [homebrew](https://brew.sh). (Note: The ns3 backend has not yet been confirmed to build within macOS)
```bash
$ brew update
$ brew upgrade
$ brew install boost cmake coreutils 
```

Then, you have to install protobuf 3.6.1 locally. You can download protobuf 3.6.1 here: [[GitHub]](https://github.com/protocolbuffers/protobuf/releases/tag/v3.6.1) [[protobuf-all-3.6.1.tar.gz]](https://github.com/protocolbuffers/protobuf/releases/download/v3.6.1/protobuf-all-3.6.1.tar.gz).
```bash
# Installing protobuf 3.6.1 locally
$ ./configure
$ make -j$(nproc)
$ make check -j$(nproc)  # checking compilation finished successfully
$ sudo make install  # register protobuf to PATH
$ which protoc  # system should be able to locate protoc
$ protoc --version  # should be 3.6.1
```

- #### Windows
ASTRA-sim is not natively supporting Windows environment at this moment. We suggest to use Docker or Windows Subsystem for Linux ([WSL](https://learn.microsoft.com/en-us/windows/wsl/install)).

#### (ii) Installing Required Python Packages
Now, you can install required Python packages, either through conda or pip3.

- #### Conda
If you are managing Python environments through conda, you can run below commands to create a new environment for astra-sim.
```bash
$ conda create -n astra-sim python=3.7
$ conda activate astra-sim
$ conda install protobuf=3.6.1 graphviz python-graphviz pydot
```

- #### pip3
You can also install required Python packages natively using pip3.
```bash
$ pip3 install --upgrade pip
$ pip3 install protobuf==3.6.1 pydot
```

#### (iii) Downloading ASTRA-sim

Once the packages are installed, you will need to clone this repository onto your local machine using the following command:
```bash
$ git clone --recurse-submodules git@github.com:astra-sim/astra-sim.git
$ cd ./astra-sim/
```

#### (iv) Compiling ASTRA-sim
Then, based on your target network backend, execute the corresponding build script:
```bash
# For the analytical network backend
$ ./build/astra_analytical/build.sh

# For the ns3 network backend. Python2 required.
$ ./build/astra_ns3/build.sh -c
```

### 2. Build ASTRA-sim in a Docker Image
Alternatively, you can build ASTRA-sim within a Docker container. (Note: The ns3 backend has not yet been confirmed to build within a Docker image)

Start by cloning this repository to your local machine using the same command as above:
```bash
$ git clone --recurse-submodules git@github.com:astra-sim/astra-sim.git
$ cd ./astra-sim/
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
$ ./build/astra_analytical/build.sh
```

## Running ASTRA-sim
Once ASTRA-sim is built, conduct experiments by passing the required configurations.
You might need to provide additional configurations based on the network backend.
The following configurations are mandatory:
* --workload-configuration: Path prefix to the execution trace. The naming rule for execution traces follows the format `{path prefix}.{npu_id}.et`. This argument provides the path prefix.
* --system-configuration: Path to the system configuration. Example system configurations can be found at `./inputs/system/`.
* --network-configuration: Path to the network configuration Example network configurations can be found at `./inputs/network/`.

Execution traces can be created using Chakra tools. You have the option of using either [the execution trace generator (et_generator)](https://github.com/chakra-et/chakra#execution-trace-generator-et_generator)
or [the execution trace converter (et_converter)](https://github.com/chakra-et/chakra#execution-trace-converter-et_converter).
The et_generator can be used to define and generate any execution traces, functioning as a test case generator. Meanwhile, the et_converter is a trace schema conversion tool, supporting PyTorch and FlexFlow execution traces, as well as ASTRA-sim 1.0 input files.

### Using the Execution Trace Generator
You can generate execution traces with et_generator with the following commands.
```bash
$ cd ./extern/graph_frontend/chakra/et_generator
$ cmake . && make -j$(nproc)
$ ./et_generator --num_npus 64 --num_dims 1
```

To run one of the example traces (`oneCommNodeAllReduce`), execute the following command.
```bash
# For the analytical network backend
$ cd -
$ ./build/astra_analytical/build/bin/AstraSim_Analytical_Congestion_Unaware \
  --workload-configuration=./extern/graph_frontend/chakra/et_generator/oneCommNodeAllReduce \
  --system-configuration=./inputs/system/Switch.json \
  --network-configuration=./inputs/network/analytical/Switch.yml \
  --remote-memory-configuration=./inputs/remote_memory/analytical/no_memory_expansion.json

# For the ns3 network backend. Python2 required.
# After editing the configuration files in the following script
$ ./build/astra_ns3/build.sh -r

# Or, alternatively:
$ cd ./extern/network_backend/ns3/simulation
$ ./waf --run "scratch/AstraSimNetwork \
  --workload-configuration=../../../../extern/graph_frontend/chakra/et_generator/oneCommNodeAllReduce \
  --system-configuration=../../../../inputs/system/Switch.json \
  --network-configuration=mix/config.txt \
  --remote-memory-configuration=../../../../inputs/remote_memory/analytical/no_memory_expansion.json \
  --logical-topology-configuration=../../../../inputs/network/ns3/sample_64nodes_1D.json \
  --comm-group-configuration=\"empty\""
$ cd -
```

Upon completion, ASTRA-sim will display the number of cycles it took to run the simulation.
```bash
sys[0] finished, 50904 cycles
sys[1] finished, 50904 cycles
...
sys[62] finished, 50904 cycles
sys[63] finished, 50904 cycles
```

### Using the Execution Trace Converter
You can convert ASTRA-sim 1.0 text input files into Chakra traces with the following commands.
```bash
$ cd ./extern/graph_frontend/chakra/
$ python3 setup.py install --user
$ python3 -m et_converter.et_converter \
    --input_type Text \
    --input_filename ../../../inputs/workload/ASTRA-sim-1.0/Resnet50_DataParallel.txt \
    --output_filename ../../../inputs/workload/ASTRA-sim-2.0/Resnet50_DataParallel \
    --num_npus 64 \
    --num_dims 1 \
    --num_passes 1
```

Run the following command.
```bash
$ cd -
$ ./build/astra_analytical/build/bin/AstraSim_Analytical_Congestion_Unaware \
  --workload-configuration=./inputs/workload/ASTRA-sim-2.0/Resnet50_DataParallel \
  --system-configuration=./inputs/system/Switch.json \
  --network-configuration=./inputs/network/analytical/Switch.yml \
  --remote-memory-configuration=./inputs/remote_memory/analytical/no_memory_expansion.json

# Similarly, for ns3, run the previous command while only changing the workload.
```

Upon completion, ASTRA-sim will display the number of cycles it took to run the simulation.
```bash
sys[62] finished, 6749042 cycles
sys[61] finished, 6749042 cycles
...
sys[0] finished, 6749042 cycles
sys[63] finished, 6749042 cycles
```

## Features Under Active Development
We are constantly working to improve ASTRA-sim and expand its capabilities. Here are some of the features that are currently under active development:

* Network Backends
    * Congestion-aware Analytical
    * Garnet (for chiplet fabrics)
* Detailed Statistics Report (Network Utilization)
  
Please note that these features are under active development and, while we aim to have them available as soon as possible, the completion timeline can vary. Check back regularly for updates on the progress of these and other features. This is an open-source project and we also value PRs from the community on features they have added.

We appreciate your interest and support in ASTRA-sim! 


## Contact Us
For any questions about using ASTRA-sim, you can email the ASTRA-sim User Mailing List: astrasim-users@googlegroups.com

To join the mailing list, please fill out the following form: https://forms.gle/18KVS99SG3k9CGXm6

This project is a collaboration of dedicated professionals. The core developers and contributors are acknowledged below.

| Developer | Organization | Responsibility | Contact |
|-----------|--------------|----------------|---------|
| Saeed Rashidi | Hewlett Packard Labs | ASTRA-sim 1.0, system layer, collective communication | rashidi1saeid@gmail.com |
| William Won | Georgia Tech | Network layer | william.won@gatech.edu |
| Taekyung Heo | Georgia Tech | Chakra, workload layer, graph execution engine, memory API | taekyung@gatech.edu |
| Changhai Man | Georgia Tech | Chakra | cman8@gatech.edu |
| Jinsun Yoo   | Georgia Tech | NS3 Network Layer Integration | jinsun@gatech.edu |
| Srinivas Sridharan | Meta | Chakra, General inquiries | srinivas@mlcommons.org |
| Tushar Krishna | Georgia Tech | General inquiries | tushar@ece.gatech.edu |
