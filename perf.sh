# !/bin/sh
perf record -e cycles -g -o perf.data ./benchmark --test="map test 2"
perf report -i perf.data > perf_report.txt
perf annotate -i perf.data > perf_annotate.txt
perf stat -e cycles,instructions -o perf_stat.txt ./benchmark --test="map test 2"
