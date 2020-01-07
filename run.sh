#!/bin/sh
python3 -m py_compile tests/test.py
./proc.py tests/__pycache__/test.cpython-37.pyc
./link.sh
