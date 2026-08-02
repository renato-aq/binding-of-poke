CC := cc
BUILD_DIR ?= build/release
TARGET := $(BUILD_DIR)/bind-of-poke
SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:src/%.c=$(BUILD_DIR)/%.o)
TEST_TARGET := $(BUILD_DIR)/test-game
TEST_OBJECTS := $(BUILD_DIR)/test_game.o $(BUILD_DIR)/game.o \
	$(BUILD_DIR)/floor.o $(BUILD_DIR)/rng.o $(BUILD_DIR)/save.o
DEPFILES := $(OBJECTS:.o=.d) $(BUILD_DIR)/test_game.d

CPPFLAGS := $(shell sdl2-config --cflags)
BASE_CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -Werror
CFLAGS ?= $(BASE_CFLAGS) -O2
LDLIBS := $(shell sdl2-config --libs) -lm

.PHONY: all run test debug sanitize stress check clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

$(BUILD_DIR)/test_game.o: tests/test_game.c | $(BUILD_DIR)
	$(CC) -Isrc $(CFLAGS) -MMD -MP -c $< -o $@

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) -lm -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

debug:
	$(MAKE) BUILD_DIR=build/debug CFLAGS="$(BASE_CFLAGS) -O0 -g3" all

sanitize:
	mkdir -p build/sanitize
	$(CC) -Isrc $(BASE_CFLAGS) -O1 -g -fsanitize=address,undefined \
		tests/test_game.c src/game.c src/floor.c src/rng.c src/save.c -lm \
		-o build/sanitize/test-game
	./build/sanitize/test-game

stress:
	$(MAKE) BUILD_DIR=build/stress CFLAGS="$(BASE_CFLAGS) -O2 -DSTRESS_TESTS" test

check: all test debug sanitize stress

clean:
	rm -rf build

-include $(DEPFILES)
