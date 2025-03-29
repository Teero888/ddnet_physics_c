CC = gcc
JUPSTAR = -march=znver5 -mtune=generic
GENERAL = -march=native -mtune=native
LTO = -flto $(GENERAL)
OPT = -O3 -mavx2 -mfma -msse4.1 -funroll-loops -mfpmath=sse -fomit-frame-pointer -fno-trapping-math -fno-signed-zeros
BASE_CFLAGS = -g -std=c99 -Wall -Wextra -I./src -I./libs/ddnet_maploader_c
LDFLAGS = -lm -lz
# -fsanitize=address
SRC_FILES = $(wildcard src/*.c)
TARGETS = benchmark example validate bench_movebox bench_intersect

define TARGET_RULE
$(1)_OBJS = $(patsubst src/%.c,obj/$(1)/%.o,$(SRC_FILES)) obj/$(1)/map_loader.o obj/$(1)/$(1).o
$(1): $$($(1)_OBJS)
	$(CC) -o $$@ $$^ $(LDFLAGS)
obj/$(1)/%.o: src/%.c | obj/$(1)
	$(CC) $(if $(filter $(1),validate),-O0 $(BASE_CFLAGS),$(OPT) $(BASE_CFLAGS)) -c -o $$@ $$<
obj/$(1)/map_loader.o: libs/ddnet_maploader_c/map_loader.c | obj/$(1)
	$(CC) $(if $(filter $(1),validate),-O0 $(BASE_CFLAGS),$(OPT) $(BASE_CFLAGS)) -c -o $$@ $$<
obj/$(1)/$(1).o: tests/$(1).c | obj/$(1)
	$(CC) $(if $(filter $(1),validate),-O0 $(BASE_CFLAGS),$(OPT) $(BASE_CFLAGS)) -c -o $$@ $$<
obj/$(1):
	mkdir -p $$@
endef

$(foreach target,$(TARGETS),$(eval $(call TARGET_RULE,$(target))))

all: $(TARGETS)

clean:
	rm -rf obj $(TARGETS)

.PHONY: all clean
