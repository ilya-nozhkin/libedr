#!/bin/bash

if [ -z "$1" ]
  then
    echo "Provide installation path as the first argument"
    exit
fi

git clone --depth 1 --branch v4.4.1 https://github.com/swig/swig.git
cd swig

mkdir build
cd build

cmake -DCMAKE_INSTALL_PREFIX=$1 ..
cmake --build . -j $(nproc) --target install

cd ../../
rm -rf ./swig
