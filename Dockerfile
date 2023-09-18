# Ubuntu
FROM ubuntu:20.04

# Maintainers
LABEL maintainer="Taekyung Heo <taekyung@gatech.edu>"
LABEL maintainer="Will Won <william.won@gatech.edu>"

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt -y update
RUN apt -y upgrade
RUN apt -y install \
    tcsh vim gcc g++ make cmake \
    libboost-dev libboost-program-options-dev \
    libprotobuf-dev protobuf-compiler \
    python3 python3-pip git

# Install python dependencies
RUN pip3 install --upgrade pip
RUN pip3 install protobuf==3.6.1 pydot

# Copy ASTRA-sim files
RUN mkdir -p /app/astra-sim/
WORKDIR /app/astra-sim/
COPY . .
