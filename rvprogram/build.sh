#!/bin/bash
set -e

export CC=riscv64-unknown-elf-gcc
export CXX=riscv64-unknown-elf-g++

mkdir -p build
pushd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake
make -j4
popd
