FROM ubuntu:xenial

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get -y upgrade && apt-get install -y \
    build-essential \
    autotools-dev \
    automake \
    libtool \
    pkg-config \
    check \
    doxygen \
    valgrind \
    xsltproc \
    libssl-dev \
    libedit-dev \
    valgrind

RUN add-apt-repository -y ppa:cleishm/neo4j
RUN apt-get update
RUN apt-get install -y libcypher-parser-dev

RUN apt-get install -y software-properties-common

RUN useradd -m build
USER build
WORKDIR /home/build
