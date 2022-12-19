#!/bin/bash
usage="$(basename "$0") [-h] [-l launch-file] [-b true|false] [-m true|false] [-s true|false] [-g log_dir] [-o output_folder]
-- this script runs menteebot light code

where:
    -h print usage
    "

compile_py=0

while getopts "ph:" flag
do
    case "${flag}" in
        p) compile_py=1;;
        h) echo "$usage"
          exit;;
        *) echo "usage: $0 [-r]" >&2
           exit;;
    esac
done

rm -rf build
mkdir build
cd build

if [ $compile_py -eq 1 ]; then
    echo "#############################Build pybind#################################33"
    cmake .. -DCANDLE_BUILD_PYTHON=TRUE    
else
    echo "#############################Build C++ library#################################33"
    cmake ..
fi

make

file_name=""

if [ $compile_py -eq 1 ]; then
    echo "Installing pybind via pip"
    cp pyCandle/pyCandle* ../candle_pip/src/mab/
    for entry in '../candle_pip/src/mab/'*
    do
        if [[ "$entry" == *"pyCandle.cpython"* ]];then
            arrIN=(${entry//// })
            file_name=${arrIN[-1]}
        fi
    done

    cd ../candle_pip/

    ##grep -E -o "*\('mab': \['*" ./setup.py

    sed -i "s/pyCandle.cpython-38-x86_64-linux-gnu.so/${file_name}/" ./setup.py
    #sed -E "s/('mab': \[')(\w*)('\])/\1$file_name\3/g" ./setup.py
    pip install . --user
else
    cp ../build/libcandle.so ../../ros2_workspace/candle_ros2/lib/
    cp -r ../include/* ../../ros2_workspace/candle_ros2/include/Candle/
fi

echo "I am DONE"
cd ..