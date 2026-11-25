CC = cc
MEMTRACKER ?= ../..
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -Werror

notes: notes.c $(MEMTRACKER)/include/memory_tracker.h $(MEMTRACKER)/lib/libmemtracker.a
	$(CC) $(CPPFLAGS) -I"$(MEMTRACKER)/include" $(CFLAGS) $(LDFLAGS) notes.c -L"$(MEMTRACKER)/lib" -lmemtracker $(LDLIBS) -o $@

.PHONY: clean
clean:
	$(RM) notes
