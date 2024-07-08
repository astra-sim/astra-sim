## Use Ubuntu
FROM ubuntu:24.04
LABEL maintainer="Will Won <william.won@gatech.edu>"


### ================== System Setups ======================
## Install System Dependencies
ENV DEBIAN_FRONTEND=noninteractive
RUN apt -y update
RUN apt -y upgrade
RUN apt -y install \
    coreutils wget vim git \
    gcc g++ \
    make cmake \
    libboost-dev libboost-program-options-dev \
    python3.12 python3-pip python3-venv

## Create Python venv: Required for Python 3.12
RUN python3 -m venv /opt/venv/astra-sim
ENV PATH="/opt/venv/astra-sim/bin:$PATH"
RUN pip3 install --upgrade pip

## Prepare ssh key for Docker image build
RUN mkdir -p -m 0600 /root/.ssh
RUN ssh-keyscan github.com >> /root/.ssh/known_hosts
### ======================================================


### ====== Abseil Installation: Protobuf Dependency ======
## Download Abseil 20240116.2 (Latest LTS as of 7/8/2024)
WORKDIR /opt
RUN wget https://github.com/abseil/abseil-cpp/releases/download/20240116.2/abseil-cpp-20240116.2.tar.gz
RUN tar -xf abseil-cpp-20240116.2.tar.gz
RUN rm abseil-cpp-20240116.2.tar.gz

## Compile Abseil
WORKDIR /opt/abseil-cpp-20240116.2/build
RUN cmake .. \
    -DCMAKE_CXX_STANDARD=14 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="/opt/abseil-cpp-20240116.2/install"
RUN cmake --build . --target install --config Release --parallel $(nproc)
ENV absl_DIR="/opt/abseil-cpp-20240116.2/install"
### ======================================================


### ============= Protobuf Installation ==================
## Download Protobuf 25.3 (=v4.25.3, latest version before protobuf v5)
WORKDIR /opt
RUN wget https://github.com/protocolbuffers/protobuf/releases/download/v25.3/protobuf-25.3.tar.gz
RUN tar -xf protobuf-25.3.tar.gz
RUN rm protobuf-25.3.tar.gz

## Compile Protobuf
WORKDIR /opt/protobuf-25.3/build
RUN cmake .. \
    -DCMAKE_CXX_STANDARD=14 \
    -DCMAKE_BUILD_TYPE=Release \
    -Dprotobuf_BUILD_TESTS=OFF \
    -Dprotobuf_ABSL_PROVIDER=package \
    -DCMAKE_INSTALL_PREFIX="/opt/protobuf-25.3/install"
RUN cmake --build . --target install --config Release --parallel $(nproc)
ENV PATH="/opt/protobuf-25.3/install/bin:$PATH"
ENV protobuf_DIR="/opt/protobuf-25.3/install"

# Also, install Python protobuf package
RUN pip3 install protobuf==4.25.3
### ======================================================


### ================== ASTRA-sim =========================
## Clone ASTRA-sim
WORKDIR /app
RUN --mount=type=ssh git clone --recurse-submodules git@github.com:astra-sim/astra-sim.git

## Install Chakra (Python module)
WORKDIR /app/astra-sim/extern/graph_frontend/chakra
RUN pip3 install .

## Move to the top ASTRA-sim directory
WORKDIR /app/astra-sim
### ======================================================
