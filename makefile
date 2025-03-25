CC = gcc
DBG = -O0 -g
OPT = -O3 -march=native -mavx2 -mfma -msse4.1 -funroll-loops -mfpmath=sse -fno-trapping-math -fno-signed-zeros
CFLAGS = $(OPT) -std=c99 -Wall -Wextra -I./src -I./libs/ddnet_maploader_c
LDFLAGS = -lm -lz

SRC_OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))
LIB_OBJS = libs/ddnet_maploader_c/map_loader.o

all: benchmark example validate

benchmark: $(SRC_OBJS) $(LIB_OBJS) tests/benchmark.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

example: $(SRC_OBJS) $(LIB_OBJS) tests/example.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

validate: $(SRC_OBJS) $(LIB_OBJS) tests/validate.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SRC_OBJS) $(LIB_OBJS) tests/*.o benchmark example validate

.PHONY: all clean
