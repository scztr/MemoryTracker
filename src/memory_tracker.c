#include "memory_tracker.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    void *pointer;
    size_t size;
    const char *file;
    int line;
} Allocation;

static Allocation records[MT_CAPACITY];
static MtStats stats;
static size_t failure_index;

int mt_reset(size_t fail_at)
{
    if (stats.active_blocks != 0) {
        return -1;
    }
    stats = (MtStats){0};
    failure_index = fail_at;
    return 0;
}

void *mt_malloc(size_t size, const char *file, int line)
{
    if (size == 0 || stats.active_blocks == MT_CAPACITY ||
        size > SIZE_MAX - stats.active_bytes) {
        stats.errors++;
        errno = size == 0 ? EINVAL : ENOMEM;
        return NULL;
    }
    stats.attempts++;
    if (failure_index != 0 && stats.attempts == failure_index) {
        stats.injected_failures++;
        errno = ENOMEM;
        return NULL;
    }
    void *pointer = malloc(size);
    if (pointer == NULL) {
        stats.system_failures++;
        return NULL;
    }
    records[stats.active_blocks++] = (Allocation){pointer, size, file, line};
    stats.allocations++;
    stats.active_bytes += size;
    if (stats.active_bytes > stats.peak_bytes) {
        stats.peak_bytes = stats.active_bytes;
    }
    return pointer;
}

void mt_free(void *pointer)
{
    if (pointer == NULL) {
        return;
    }
    for (size_t i = 0; i < stats.active_blocks; i++) {
        if (records[i].pointer == pointer) {
            size_t bytes = records[i].size;
            records[i] = records[stats.active_blocks - 1];
            records[stats.active_blocks - 1] = (Allocation){0};
            stats.active_blocks--;
            stats.active_bytes -= bytes;
            stats.frees++;
            free(pointer);
            return;
        }
    }
    stats.errors++;
}

MtStats mt_stats(void)
{
    return stats;
}

int mt_first_active(const char **file, int *line, size_t *size)
{
    if (stats.active_blocks == 0) {
        return 0;
    }
    *file = records[0].file;
    *line = records[0].line;
    *size = records[0].size;
    return 1;
}

void mt_report(FILE *stream)
{
    for (size_t i = 0; i < stats.active_blocks; i++) {
        fprintf(stream, "%s:%d: %zu bytes still allocated\n",
                records[i].file, records[i].line, records[i].size);
    }
    fprintf(stream, "Active: %zu blocks, %zu bytes; peak: %zu bytes\n",
            stats.active_blocks, stats.active_bytes, stats.peak_bytes);
}
