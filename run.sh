#!/bin/sh
python3 -m py_compile test.py
./proc.py __pycache__/test.cpython-37.pyc
./link.sh
