CC = gcc
LTO = -flto -march=native -mtune=native
OPT = -O3 -mavx2 -mfma -msse4.1 -funroll-loops -mfpmath=sse -fno-trapping-math -fno-signed-zeros
BASE_CFLAGS = -g -std=c99 -Wall -Wextra -I./src -I./libs/ddnet_maploader_c
LDFLAGS = -lm -lz

# source files
SRC_FILES = $(wildcard src/*.c)
LIB_FILE = libs/ddnet_maploader_c/map_loader.c

# object directories
BENCHMARK_OBJDIR = obj/benchmark
EXAMPLE_OBJDIR = obj/example
VALIDATE_OBJDIR = obj/validate
BENCH_MOVEBOX_OBJDIR = obj/bench_movebox

# object files for each target
BENCHMARK_OBJS = $(patsubst src/%.c,$(BENCHMARK_OBJDIR)/%.o,$(SRC_FILES)) $(BENCHMARK_OBJDIR)/map_loader.o $(BENCHMARK_OBJDIR)/benchmark.o
EXAMPLE_OBJS = $(patsubst src/%.c,$(EXAMPLE_OBJDIR)/%.o,$(SRC_FILES)) $(EXAMPLE_OBJDIR)/map_loader.o $(EXAMPLE_OBJDIR)/example.o
VALIDATE_OBJS = $(patsubst src/%.c,$(VALIDATE_OBJDIR)/%.o,$(SRC_FILES)) $(VALIDATE_OBJDIR)/map_loader.o $(VALIDATE_OBJDIR)/validate.o
BENCH_MOVEBOX_OBJS = $(patsubst src/%.c,$(BENCH_MOVEBOX_OBJDIR)/%.o,$(SRC_FILES)) $(BENCH_MOVEBOX_OBJDIR)/map_loader.o $(BENCH_MOVEBOX_OBJDIR)/bench_movebox.o

# target-specific CFLAGS
BENCHMARK_CFLAGS = $(OPT) $(BASE_CFLAGS)
EXAMPLE_CFLAGS = $(OPT) $(BASE_CFLAGS)
VALIDATE_CFLAGS = -O0 $(BASE_CFLAGS)
BENCH_MOVEBOX_CFLAGS =  $(OPT) $(BASE_CFLAGS)

all: benchmark example validate bench_movebox

# benchmark target
benchmark: $(BENCHMARK_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# example target
example: $(EXAMPLE_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# validate target
validate: $(VALIDATE_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# bench_movebox target
bench_movebox: $(BENCH_MOVEBOX_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# rules to compile source files into object files
$(BENCHMARK_OBJDIR)/%.o: src/%.c | $(BENCHMARK_OBJDIR)
	$(CC) $(BENCHMARK_CFLAGS) -c -o $@ $<

$(BENCHMARK_OBJDIR)/map_loader.o: libs/ddnet_maploader_c/map_loader.c | $(BENCHMARK_OBJDIR)
	$(CC) $(BENCHMARK_CFLAGS) -c -o $@ $<

$(BENCHMARK_OBJDIR)/benchmark.o: tests/benchmark.c | $(BENCHMARK_OBJDIR)
	$(CC) $(BENCHMARK_CFLAGS) -c -o $@ $<

$(EXAMPLE_OBJDIR)/%.o: src/%.c | $(EXAMPLE_OBJDIR)
	$(CC) $(EXAMPLE_CFLAGS) -c -o $@ $<

$(EXAMPLE_OBJDIR)/map_loader.o: libs/ddnet_maploader_c/map_loader.c | $(EXAMPLE_OBJDIR)
	$(CC) $(EXAMPLE_CFLAGS) -c -o $@ $<

$(EXAMPLE_OBJDIR)/example.o: tests/example.c | $(EXAMPLE_OBJDIR)
	$(CC) $(EXAMPLE_CFLAGS) -c -o $@ $<

$(VALIDATE_OBJDIR)/%.o: src/%.c | $(VALIDATE_OBJDIR)
	$(CC) $(VALIDATE_CFLAGS) -c -o $@ $<

$(VALIDATE_OBJDIR)/map_loader.o: libs/ddnet_maploader_c/map_loader.c | $(VALIDATE_OBJDIR)
	$(CC) $(VALIDATE_CFLAGS) -c -o $@ $<

$(VALIDATE_OBJDIR)/validate.o: tests/validate.c | $(VALIDATE_OBJDIR)
	$(CC) $(VALIDATE_CFLAGS) -c -o $@ $<

$(BENCH_MOVEBOX_OBJDIR)/%.o: src/%.c | $(BENCH_MOVEBOX_OBJDIR)
	$(CC) $(BENCH_MOVEBOX_CFLAGS) -c -o $@ $<

$(BENCH_MOVEBOX_OBJDIR)/map_loader.o: libs/ddnet_maploader_c/map_loader.c | $(BENCH_MOVEBOX_OBJDIR)
	$(CC) $(BENCH_MOVEBOX_CFLAGS) -c -o $@ $<

$(BENCH_MOVEBOX_OBJDIR)/bench_movebox.o: tests/bench_movebox.c | $(BENCH_MOVEBOX_OBJDIR)
	$(CC) $(BENCH_MOVEBOX_CFLAGS) -c -o $@ $<

# create object directories
$(BENCHMARK_OBJDIR) $(EXAMPLE_OBJDIR) $(VALIDATE_OBJDIR) $(BENCH_MOVEBOX_OBJDIR):
	mkdir -p $@

# clean up
clean:
	rm -rf obj benchmark example validate bench_movebox

.PHONY: all clean
