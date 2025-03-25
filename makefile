CC = gcc
OPT = -O3 -march=native -mavx2 -mfma -msse4.1 -funroll-loops -mfpmath=sse -fno-trapping-math -fno-signed-zeros -g
BASE_CFLAGS = -std=c99 -Wall -Wextra -I./src -I./libs/ddnet_maploader_c
LDFLAGS = -lm -lz

# source files
SRC_FILES = $(wildcard src/*.c)
LIB_FILE = libs/ddnet_maploader_c/map_loader.c

# object directories
BENCHMARK_OBJDIR = obj/benchmark
EXAMPLE_OBJDIR = obj/example
VALIDATE_OBJDIR = obj/validate

# object files for each target
BENCHMARK_OBJS = $(patsubst src/%.c,$(BENCHMARK_OBJDIR)/%.o,$(SRC_FILES)) $(BENCHMARK_OBJDIR)/map_loader.o $(BENCHMARK_OBJDIR)/benchmark.o
EXAMPLE_OBJS = $(patsubst src/%.c,$(EXAMPLE_OBJDIR)/%.o,$(SRC_FILES)) $(EXAMPLE_OBJDIR)/map_loader.o $(EXAMPLE_OBJDIR)/example.o
VALIDATE_OBJS = $(patsubst src/%.c,$(VALIDATE_OBJDIR)/%.o,$(SRC_FILES)) $(VALIDATE_OBJDIR)/map_loader.o $(VALIDATE_OBJDIR)/validate.o

# target-specific CFLAGS
BENCHMARK_CFLAGS = $(BASE_CFLAGS) $(OPT) -DNO_COLLISION_CLAMP
EXAMPLE_CFLAGS = $(BASE_CFLAGS) $(OPT)
VALIDATE_CFLAGS = $(BASE_CFLAGS) $(OPT)

all: benchmark example validate

# benchmark target
benchmark: $(BENCHMARK_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# example target
example: $(EXAMPLE_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# validate target
validate: $(VALIDATE_OBJS)
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

# create object directories
$(BENCHMARK_OBJDIR) $(EXAMPLE_OBJDIR) $(VALIDATE_OBJDIR):
	mkdir -p $@

# clean up
clean:
	rm -rf obj benchmark example validate

.PHONY: all clean
