#!/bin/sh
clang -c runtime.c -S -emit-llvm
llvm-link foo.il runtime.ll -S > linked.ll
llc linked.ll
clang linked.s

