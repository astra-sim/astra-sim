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
    openmpi-bin openmpi-doc libopenmpi-dev \
    python3.11 python3-pip python3-venv \
    graphviz

## Create Python venv: Required for Python 3.11
RUN python3 -m venv /opt/venv/astra-sim
ENV PATH="/opt/venv/astra-sim/bin:$PATH"
RUN pip3 install --upgrade pip

## Add astra-sim to PYTHONPATH
ENV PYTHONPATH="/app/astra-sim"

# STG dependencies (Python protobuf for Chakra trace tooling; C++ protobuf is built in-tree)
RUN pip3 install numpy sympy graphviz pandas "protobuf>=6.31.0,<7"
### ======================================================


### ================== Finalize ==========================
## Move to the application directory
WORKDIR /app/astra-sim
### ======================================================
