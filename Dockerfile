FROM ubuntu:20.04

MAINTAINER Chun-Yi Liu <liu225269@gmail.com>

USER root

RUN apt-get update && DEBIAN_FRONTEND="noninteractive" TZ="America/New_York" apt-get install -y tzdata

RUN apt-get update \
 && apt install --no-install-recommends -y build-essential g++ libboost-all-dev libaio-dev wget tar vim libtbb-dev \
 && rm -rf /var/lib/apt/lists/*

COPY ./ MBFGraph

RUN cd MBFGraph \
  && make clean \
  && make -j

CMD /bin/bash --login
