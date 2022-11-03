#!/bin/bash
usage="$(basename "$0") [-h] [-l launch-file] [-b true|false] [-m true|false] [-s true|false] [-g log_dir] [-o output_folder]
-- this script runs menteebot light code

where:
    -p create candlePy.so to create pybind and also install with pip. No longer supported
    -h print usage
    "

compile_py=0

while getopts "h:" flag
do
    case "${flag}" in
        h) echo "$usage"
          exit;;
        *) echo "usage: $0 [-r]" >&2
           exit;;
    esac
done

rm -rf build
mkdir build
cd build

echo "#############################Build C++ library#################################33"
cmake ..

make

cp Candle/libcandle.so ../../ros2_workspace/candle_ros2/lib/
cp -r ../Candle/include/* ../../ros2_workspace/candle_ros2/include/Candle/

echo "I am DONE"
cd ..