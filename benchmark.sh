#!/usr/bin/env bash
export CXX=clang++-12
set -e
mkdir -p $HOME/pgo

mkdir -p build_profile_clang
pushd build_profile_clang
cmake .. -DPROFILING=ON -DPGO=ON -DCMAKE_BUILD_TYPE=Release
make -j16
./bench
popd

llvm-profdata-12 merge -output=$HOME/pgo/default.profdata $HOME/pgo/*.profraw

mkdir -p build_clang
pushd build_clang
cmake .. -DPROFILING=OFF -DPGO=ON -DCMAKE_BUILD_TYPE=Release
make -j16
./bench
popd
