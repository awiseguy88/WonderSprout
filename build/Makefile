CC ?= gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -I../src/core -O2
LDFLAGS = -lm
CORE_SRC = ../src/core/curriculum.c ../src/core/progress.c ../src/core/storage.c ../src/core/audio_synth.c
TEST_SRC = ../src/test/test_core.c

all: test

test: test_core
	./test_core

test_core: $(CORE_SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $(CORE_SRC) $(TEST_SRC) $(LDFLAGS)

.PHONY: all test clean
clean:
	rm -f test_core
