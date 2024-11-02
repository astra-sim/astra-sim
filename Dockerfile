## ******************************************************************************
## This source code is licensed under the MIT license found in the
## LICENSE file in the root directory of this source tree.
##
## Copyright (c) 2024 Georgia Institute of Technology
## ******************************************************************************

## Use Ubuntu
FROM ubuntu:22.04
LABEL maintainer="Will Won <william.won@gatech.edu>"
LABEL maintainer="Jinsun Yoo <jinsun@gatech.edu>"


### ================== System Setups ======================
## Install System Dependencies
ENV DEBIAN_FRONTEND=noninteractive
RUN apt -y update
RUN apt -y install \
    coreutils wget vim git \
    gcc g++ clang-format \
    make cmake \
    libboost-dev libboost-program-options-dev \
    python3.11 python3-pip python3-venv

## Create Python venv: Required for Python 3.11
RUN python3 -m venv /opt/venv/astra-sim
ENV PATH="/opt/venv/astra-sim/bin:$PATH"
RUN pip3 install --upgrade pip
### ======================================================


### ====== Abseil Installation: Protobuf Dependency ======
## Download Abseil 20240722.0 (Latest LTS as of 10/31/2024)
ARG ABSL_VER=20240722.0

# Download source
WORKDIR /opt
RUN wget https://github.com/abseil/abseil-cpp/releases/download/${ABSL_VER}/abseil-cpp-${ABSL_VER}.tar.gz
RUN tar -xf abseil-cpp-${ABSL_VER}.tar.gz
RUN rm abseil-cpp-${ABSL_VER}.tar.gz

## Compile Abseil
WORKDIR /opt/abseil-cpp-${ABSL_VER}/build
RUN cmake .. \
    -DCMAKE_CXX_STANDARD=14 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="/opt/abseil-cpp-${ABSL_VER}/install"
RUN cmake --build . --target install --config Release --parallel $(nproc)
ENV absl_DIR="/opt/abseil-cpp-${ABSL_VER}/install"
### ======================================================


### ============= Protobuf Installation ==================
## Download Protobuf 28.3 (=v5.28.3, latest version as of 10/31/2024)
ARG PROTOBUF_VER=28.3

# Download source
WORKDIR /opt
RUN wget https://github.com/protocolbuffers/protobuf/releases/download/v${PROTOBUF_VER}/protobuf-${PROTOBUF_VER}.tar.gz
RUN tar -xf protobuf-${PROTOBUF_VER}.tar.gz
RUN rm protobuf-${PROTOBUF_VER}.tar.gz

## Compile Protobuf
WORKDIR /opt/protobuf-${PROTOBUF_VER}/build
RUN cmake .. \
    -DCMAKE_CXX_STANDARD=14 \
    -DCMAKE_BUILD_TYPE=Release \
    -Dprotobuf_BUILD_TESTS=OFF \
    -Dprotobuf_ABSL_PROVIDER=package \
    -DCMAKE_INSTALL_PREFIX="/opt/protobuf-${PROTOBUF_VER}/install"
RUN cmake --build . --target install --config Release --parallel $(nproc)
ENV PATH="/opt/protobuf-${PROTOBUF_VER}/install/bin:$PATH"
ENV protobuf_DIR="/opt/protobuf-${PROTOBUF_VER}/install"

# Also, install Python protobuf package
RUN pip3 install protobuf==5.${PROTOBUF_VER}

# Set the environment variable
ENV PROTOBUF_FROM_SOURCE=True
### ======================================================


### ================== Finalize ==========================
## Move to the application directory
WORKDIR /app/astra-sim
### ======================================================
