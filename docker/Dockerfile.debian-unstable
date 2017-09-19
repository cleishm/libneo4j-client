FROM debian:unstable

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update
RUN apt-get -y upgrade
RUN apt-get install -y build-essential debhelper dh-autoreconf devscripts
RUN apt-get install -y autotools-dev automake libtool pkg-config
RUN apt-get install -y check
RUN apt-get install -y doxygen
RUN apt-get install -y git git-buildpackage
RUN apt-get install -y libedit-dev
RUN apt-get install -y libssl-dev
RUN apt-get install -y man
RUN apt-get install -y valgrind
RUN apt-get install -y vim
RUN apt-get install -y procps
RUN apt-get install -y gnupg1
RUN apt-get install -y libcypher-parser-dev

RUN useradd -m debian
RUN echo "debian ALL=NOPASSWD:ALL" >> /etc/sudoers
ADD dput.cf /home/debian/.dput.cf
ADD devscripts /home/debian/.devscripts
RUN chown -R debian:debian /home/debian

USER debian
WORKDIR /home/debian

ENV GIT_AUTHOR_NAME="Chris Leishman" GIT_COMMITTER_NAME="Chris Leishman"
ENV EMAIL=chris@leishman.org
ENV DEBFULLNAME="Chris Leishman" DEBEMAIL=chris@leishman.org
