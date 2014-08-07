#!/bin/sh

cd "$(dirname "$0")"

export PYTHONPATH=..
python -m unittest discover -p 'Test*.py'
