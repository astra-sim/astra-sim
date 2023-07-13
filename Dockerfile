FROM ubuntu:20.04
LABEL maintainer="Taekyung Heo <taekyung@gatech.edu>"

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -y update \
    && apt-get -y install\
    tcsh vim gcc g++ make cmake\
    libboost-dev libboost-program-options-dev\
    libprotobuf-dev protobuf-compiler\
    python3 python3-pip git

RUN pip3 install protobuf==3.6.1 pydot

WORKDIR /home/
COPY . .
