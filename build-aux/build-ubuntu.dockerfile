FROM ubuntu:xenial

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update
RUN apt-get -y upgrade
RUN apt-get install -y git ssh
RUN apt-get install -y build-essential autotools-dev automake libtool pkg-config
RUN apt-get install -y doxygen
RUN apt-get install -y check
RUN apt-get install -y valgrind
RUN apt-get install -y xsltproc
RUN apt-get install -y libssl-dev
RUN apt-get install -y libedit-dev

RUN apt-get install -y software-properties-common
RUN add-apt-repository -y ppa:cleishm/neo4j
RUN apt-get update
RUN apt-get install -y libcypher-parser-dev

RUN useradd -m build
USER build
WORKDIR /home/build
