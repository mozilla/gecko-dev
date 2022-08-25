#!/usr/bin/bash

export LDFLAGS=-L${zlib-path}
export CXXFLAGS=-I${zlib-path}
export CFLAGS=-I${zlib-path}

export CC=/usr/bin/riscv64-linux-gnu-gcc
export CXX=/usr/bin/riscv64-linux-gnu-g++

./mach build
