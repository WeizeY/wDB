FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    gdb \
    valgrind \
    clang \
    clang-format \
    clang-tidy \
    libgtest-dev \
    libgmock-dev \
    git \
    vim \
    curl \
    python3 \
    strace \
    ltrace \
    perf-tools-unstable \
    && rm -rf /var/lib/apt/lists/*

# Build and install GoogleTest from source (ubuntu pkg is headers only)
RUN cd /usr/src/gtest && cmake . && make && cp lib/*.a /usr/lib/

WORKDIR /wdb

# Pre-warm cmake download cache on first build (nothing to download yet)
ENV CC=gcc
ENV CXX=g++

CMD ["/bin/bash"]
