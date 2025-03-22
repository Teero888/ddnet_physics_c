CC = gcc
CFLAGS = -g -fsanitize=address -std=c99 -Wall -Wextra -O0 -I./src -I./libs/ddnet_maploader_c
LDFLAGS = -lm -lz

SRC_OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))
LIB_OBJS = libs/ddnet_maploader_c/map_loader.o

all: benchmark example

benchmark: $(SRC_OBJS) $(LIB_OBJS) tests/benchmark.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

example: $(SRC_OBJS) $(LIB_OBJS) tests/example.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(SRC_OBJS) $(LIB_OBJS) tests/*.o benchmark example

.PHONY: all clean
