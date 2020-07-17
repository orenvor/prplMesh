FROM ubuntu:18.04

RUN apt-get update && apt-get install -y \
    autoconf \
    autogen \
    binutils \
    clang-format \
    cmake \
    curl \
    gcc \
    gcovr \
    git \
    libjson-c-dev \
    libncurses-dev \
    libnl-3-dev \
    libnl-route-3-dev \
    libnl-genl-3-dev \
    libreadline-dev \
    libssl-dev \
    libtool \
    ninja-build \
    pkg-config \
    python \
    python-yaml \
    python3 \
    python3-yaml \
    uuid-runtime \
    valgrind \
    vim
# AMBRIORIX dependencies
RUN \
    apt-get install -y \
    bison \
    flex \
    libevent-dev \
    libyajl-dev \
    repo && \
    echo "deb http://cz.archive.ubuntu.com/ubuntu eoan main universe" | tee -a  /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y \
    liburiparser-dev && \
    rm -rf /var/lib/apt/lists/*
WORKDIR ambiorix
# Fetch and intall Bus Agnostic API libs, applications
# "sed" is required to change from ssh to https protocols
# https allows anonymous download, when ssh doesn't
RUN \
    repo init -u https://gitlab.com/soft.at.home/ambiorix/ambiorix.git && \
    sed -i "s|ssh://git@gitlab.com/soft.at.home/ambiorix/|https://gitlab.com/soft.at.home/ambiorix/|g" .repo/manifests/default.xml && \
    repo sync && \
    make install -C libraries/libamxc && \
    make install -C libraries/libamxp && \
    make install -C libraries/libamxd && \
    make install -C libraries/libamxj && \
    make install -C libraries/libamxo && \
    make install -C libraries/libamxb && \
    make install -C applications/amxb-inspect && \
    make install -C applications/amxo-cg && \
    make install -C applications/amxrt
