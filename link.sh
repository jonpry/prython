#!/bin/sh
clang++ -c runtime.cpp -S -g -emit-llvm -O1
llvm-link foo.il runtime.ll -S > linked.ll
opt linked.ll -O3 -S -o linked.bc -std-link-opts 
clang linked.ll -lm -g

