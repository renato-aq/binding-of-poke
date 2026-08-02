CC := cc
TARGET := build/bind-of-poke
SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:src/%.c=build/%.o)
TEST_TARGET := build/test-game
TEST_OBJECTS := build/test_game.o build/game.o
DEPFILES := $(OBJECTS:.o=.d) build/test_game.d

CPPFLAGS := $(shell sdl2-config --cflags)
CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -Werror -O2
LDLIBS := $(shell sdl2-config --libs) -lm

.PHONY: all run test clean debug

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDLIBS) -o $@

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

build/test_game.o: tests/test_game.c | build
	$(CC) -Isrc $(CFLAGS) -MMD -MP -c $< -o $@

$(TEST_TARGET): $(TEST_OBJECTS)
	$(CC) $(TEST_OBJECTS) -lm -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

debug: CFLAGS := -std=c17 -Wall -Wextra -Wpedantic -Werror -O0 -g3
debug: clean all

clean:
	rm -rf build

-include $(DEPFILES)
