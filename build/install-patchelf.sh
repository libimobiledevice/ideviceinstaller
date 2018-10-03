#!/bin/bash

# We need patchelf 0.9; Ubuntu 16.04 includes 0.8
wget https://nixos.org/releases/patchelf/patchelf-0.9/patchelf-0.9.tar.gz
tar -xvzf patchelf-0.9.tar.gz
cd patchelf-0.9
./configure
make
cd ..
