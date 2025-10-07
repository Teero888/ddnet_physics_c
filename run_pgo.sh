#! /usr/bin/bash
# builds and runs an optimized benchmark executable
rm -rf build
mkdir build
cd build
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCMAKE_BUILD_TYPE=Release -DENABLE_AGGRESSIVE_OPTIM=On -DTESTS=On -DPGO_STAGE=GENERATE > /dev/null
make -j$(nproc) benchmark > /dev/null
./tests/optimized/benchmark > /dev/null
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=On -DCMAKE_BUILD_TYPE=Release -DENABLE_AGGRESSIVE_OPTIM=On -DTESTS=On -DPGO_STAGE=USE > /dev/null
make -j$(nproc) benchmark > /dev/null
./tests/optimized/benchmark
./tests/optimized/benchmark --multi
cd ..

