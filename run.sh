#!/bin/sh
python3 -m py_compile tests/test_c.py
./proc.py tests/__pycache__/test_c.cpython-37.pyc
./link.sh
