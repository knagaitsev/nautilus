#!/bin/bash

export NAUT_ENABLE_INTS=1 && make clean && make -j && make bitcode && make isoimage && make uImage
