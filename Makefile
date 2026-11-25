CC = cc
AR = ar
CPPFLAGS = -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -g

LIBRARY = lib/libmemtracker.a
LIB_OBJECT = build/memory_tracker.o

.PHONY: all library clean
all: $(LIBRARY)
library: $(LIBRARY)

$(LIBRARY): $(LIB_OBJECT) | lib
	$(AR) rcs $@ $<

build/%.o: src/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

build lib:
	mkdir -p $@

clean:
	$(RM) memorytracker build/*.o build/*.d $(LIBRARY)

-include $(LIB_OBJECT:.o=.d)
