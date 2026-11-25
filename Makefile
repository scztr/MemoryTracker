CC = cc
AR = ar
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g

LIBRARY = lib/libmemtracker.a
PROGRAM_OBJECTS = build/main.o build/scenarios.o
LIB_OBJECT = build/memory_tracker.o

.PHONY: all library example test clean
all: memorytracker
library: $(LIBRARY)

memorytracker: $(PROGRAM_OBJECTS) $(LIBRARY)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(LIBRARY): $(LIB_OBJECT) | lib
	$(AR) rcs $@ $<

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build lib:
	mkdir -p $@

build/test_tracker: tests/test_tracker.c include/memory_tracker.h $(LIBRARY) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LIBRARY) $(LDLIBS) -o $@

example: $(LIBRARY)
	$(MAKE) -C examples/notes

build/test_cli: tests/test_cli.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $< $(LDLIBS) -o $@

test: memorytracker build/test_tracker build/test_cli example
	./build/test_tracker
	./build/test_cli

clean:
	$(RM) memorytracker build/*.o build/*.d build/test_tracker build/test_cli $(LIBRARY)
	$(MAKE) -C examples/notes clean

-include $(PROGRAM_OBJECTS:.o=.d) $(LIB_OBJECT:.o=.d)
