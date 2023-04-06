FROM debian:unstable

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get -y upgrade && apt-get install -y \
    build-essential \
    autotools-dev \
    automake \
    libtool \
    pkg-config \
    check \
    doxygen \
    xsltproc \
    libssl-dev \
    libedit-dev \
    libcypher-parser-dev \
    valgrind

RUN useradd -m build

USER build
WORKDIR /home/build
