FROM ubuntu:20.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y git gcc make libfuse-dev pkg-config\
    && rm -rf /var/lib/apt/lists/*
RUN git clone https://github.com/jmattsson/fuseloop
WORKDIR fuseloop
RUN make clean
RUN make
