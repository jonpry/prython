#!/bin/sh
 ./c_gen.py 
gperf get_slot.gperf -C -I -t > get_slot.cpp

clang++-8 -c runtime.cpp -S -g -emit-llvm -O1
clang++-8 -c float.cpp -S -g -emit-llvm -O1
clang++-8 -c except.cpp -S -g -emit-llvm -O1 -Wno-format
clang++-8 -c int.cpp -S -g -emit-llvm -O1
clang++-8 -c dict.cpp -S -g -emit-llvm -O1

llvm-link-8 foo.ll runtime.ll except.ll float.ll int.ll dict.ll -S > linked.ll
opt-8 linked.ll -O3 -S -o linked.bc -std-link-opts 
opt-8 linked.bc -O3 -S -o linked.bc2 -std-link-opts 

clang++-8 linked.ll -lm -g -O0

