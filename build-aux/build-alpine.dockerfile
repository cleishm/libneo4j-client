FROM alpine:latest
RUN apk update && \
    apk add alpine-sdk && \
    apk add bash vim

RUN adduser -s /bin/bash -D apk && \
    addgroup apk abuild
RUN echo "apk ALL=NOPASSWD:ALL" >> /etc/sudoers
RUN mkdir -p /var/cache/distfiles && \
    chgrp abuild /var/cache/distfiles && \
    chmod g+w /var/cache/distfiles

USER apk
WORKDIR /home/apk

RUN git config --global user.name "Chris Leishman"
RUN git config --global user.email "chris@leishman.org"
RUN abuild-keygen -a -i
