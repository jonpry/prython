#!/bin/sh
clang++-8 -c runtime.cpp -S -g -emit-llvm -O1
clang++-8 -c except.cpp -S -g -emit-llvm -O1 -Wno-format

llvm-link-8 foo.ll runtime.ll except.ll -S > linked.ll
opt-8 linked.ll -O3 -S -o linked.bc -std-link-opts 
clang++-8 linked.ll -lm -g -O0

