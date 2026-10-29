# ASTRA-sim 2.0  
[ASTRA-sim](https://astra-sim.github.io/) is a distributed machine learning system simulator developed by Intel, Meta, and Georgia Tech. It enables the systematic study of challenges in modern deep learning systems, allowing for the exploration of bottlenecks and the development of efficient methodologies for large DNN models across diverse future platforms.

The previous version, ASTRA-sim 1.0, is available in the `ASTRA-sim-1.0` [branch](https://github.com/astra-sim/astra-sim/tree/ASTRA-sim-1.0).

For a comprehensive understanding of the tool, and to gain insights into its capabilities, please visit our [website](https://astra-sim.github.io/).

## ASTRA-sim Overview

Here is a concise visual summary of our simulator: For a comprehensive understanding of the tool, and to gain insights into its capabilities, please refer to our paper:![alt text](https://github.com/astra-sim/astra-sim/blob/master/docs/images/astrasim_overview_codesign.png)

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
#### Installing Dependencies
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
$ sudo pip3 install protobuf==3.6.1 pydot
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

Finally, you can install required Python packages using pip3.
```bash
$ pip3 install --upgrade pip
$ pip3 install protobuf==3.6.1 pydot
```

- #### Windows
ASTRA-sim does not natively support Windows environment at this moment. We suggest to use Docker or Windows Subsystem for Linux ([WSL](https://learn.microsoft.com/en-us/windows/wsl/install)).

#### Downloading ASTRA-sim

Once the packages are installed, you will need to clone this repository onto your local machine using the following command:
```bash
$ git clone --recurse-submodules git@github.com:astra-sim/astra-sim.git
$ cd ./astra-sim/
```

#### Compiling ASTRA-sim
Then, based on your target network backend, execute the corresponding build script:
```bash
# For the analytical network backend
$ ./build/astra_analytical/build.sh
# For the ns3 network backend. Python2 required.
$ ./build/astra_ns3/build.sh -c
```

### 2. Build ASTRA-sim in a Docker Image
Alternatively, you can build ASTRA-sim within a Docker container. (Note: The ns3 backend has not yet been confirmed to build within a Docker image)

For information on how to use ASTRA-sim, please visit our [Wiki](https://astra-sim.github.io/astra-sim-docs/index.html).

ASTRA-sim accepts Chakra Execution Traces as workload-layer inputs. For details, please visit [Chakra Github](https://github.com/mlcommons/chakra).

We appreciate your interest and support in ASTRA-sim!

## Contact Us
For any questions about using ASTRA-sim, you can email the ASTRA-sim User Mailing List: astrasim-users@googlegroups.com

To join the mailing list, please fill out the following form: https://forms.gle/18KVS99SG3k9CGXm6
