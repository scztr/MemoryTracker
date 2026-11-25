#ifndef MEMORY_TRACKER_H
#define MEMORY_TRACKER_H

#include <stddef.h>
#include <stdio.h>

#define MT_CAPACITY 128

typedef struct {
    size_t attempts;
    size_t allocations;
    size_t frees;
    size_t active_blocks;
    size_t active_bytes;
    size_t peak_bytes;
    size_t injected_failures;
    size_t system_failures;
    size_t errors;
} MtStats;

int mt_reset(size_t fail_at);
void *mt_malloc(size_t size, const char *file, int line);
void mt_free(void *pointer);
MtStats mt_stats(void);
int mt_first_active(const char **file, int *line, size_t *size);
void mt_report(FILE *stream);

#define MT_MALLOC(size) mt_malloc((size), __FILE__, __LINE__)
#define MT_FREE(pointer) mt_free((pointer))

#endif
