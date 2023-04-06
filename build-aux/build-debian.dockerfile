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
RUN apt-get install -y \
    debhelper \
    dh-autoreconf \
    devscripts \
    git \
    git-buildpackage \
    vim \
    gnupg1

RUN useradd -m build
RUN echo "build ALL=NOPASSWD:ALL" >> /etc/sudoers
ADD build-aux/debian/dput.cf /home/build/.dput.cf
ADD build-aux/debian/devscripts /home/build/.devscripts
RUN chown -R build:build /home/build

USER build
WORKDIR /home/build

ENV GIT_AUTHOR_NAME="Chris Leishman" GIT_COMMITTER_NAME="Chris Leishman"
ENV EMAIL=chris@leishman.org
ENV DEBFULLNAME="Chris Leishman" DEBEMAIL=chris@leishman.org
