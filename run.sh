#!/bin/sh
python3 -m py_compile tests/test_for.py
./proc.py tests/__pycache__/test_for.cpython-37.pyc
./link.sh
