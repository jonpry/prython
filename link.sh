#!/bin/sh
clang -c runtime.c -S -emit-llvm -O1
llvm-link foo.il runtime.ll -S > linked.ll
opt linked.ll -O3 -S -o linked.bc -std-link-opts 
llc linked.ll
clang linked.s -lm

