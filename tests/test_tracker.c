#include "memory_tracker.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    assert(mt_reset(0) == 0);
    void *a = MT_MALLOC(100);
    void *b = MT_MALLOC(40);
    assert(a && b);
    memset(a, 0x5a, 100);
    assert(mt_stats().active_blocks == 2);
    assert(mt_stats().active_bytes == 140);
    assert(mt_stats().peak_bytes == 140);
    assert(mt_reset(0) == -1);
    MT_FREE(a);
    assert(mt_stats().active_bytes == 40);
    const char *file;
    int line;
    size_t bytes;
    assert(mt_first_active(&file, &line, &bytes));
    assert(bytes == 40 && line > 0 && strstr(file, "test_tracker.c"));

    FILE *report = tmpfile();
    assert(report);
    mt_report(report);
    rewind(report);
    char text[256] = {0};
    assert(fread(text, 1, sizeof(text) - 1, report) > 0);
    assert(strstr(text, "40 bytes still allocated"));
    assert(fclose(report) == 0);
    assert(mt_stats().active_blocks == 1);
    MT_FREE(b);
    MT_FREE(NULL);
    assert(mt_stats().active_bytes == 0 && mt_stats().frees == 2);
    assert(mt_stats().peak_bytes == 140);
    assert(!mt_first_active(&file, &line, &bytes));

    assert(mt_reset(2) == 0);
    a = MT_MALLOC(10);
    b = MT_MALLOC(20);
    void *c = MT_MALLOC(30);
    assert(a && !b && c);
    assert(mt_stats().attempts == 3 && mt_stats().injected_failures == 1);
    assert(mt_stats().allocations == 2 && mt_stats().active_bytes == 40);
    MT_FREE(a);
    MT_FREE(c);

    assert(mt_reset(0) == 0);
    int untracked = 0;
    MT_FREE(&untracked);
    assert(mt_stats().errors == 1);
    assert(MT_MALLOC(0) == NULL);
    assert(mt_stats().errors == 2);

    assert(mt_reset(0) == 0);
    void *blocks[MT_CAPACITY];
    for (size_t i = 0; i < MT_CAPACITY; i++) {
        blocks[i] = MT_MALLOC(1);
        assert(blocks[i]);
    }
    assert(MT_MALLOC(1) == NULL);
    assert(mt_stats().errors == 1 && mt_stats().active_blocks == MT_CAPACITY);
    for (size_t i = 0; i < MT_CAPACITY; i++) {
        MT_FREE(blocks[i]);
    }
    assert(mt_stats().active_blocks == 0);
    assert(mt_reset(0) == 0);
    puts("Tracker tests: PASS");
    return 0;
}
