#!/bin/bash
make clean --silent
make validate benchmark --silent -j$(nproc)
./validate
taskset --cpu-list 0-32 hyperfine --warmup 10 ./benchmark
