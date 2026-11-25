#include "memory_tracker.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    int leak = argc == 2 && strcmp(argv[1], "--leak") == 0;
    if (argc > 1 && !leak) {
        fprintf(stderr, "Usage: %s [--leak]\n", argv[0]);
        return 2;
    }

    const char message[] = "Study C";
    char *note = MT_MALLOC(sizeof(message));
    if (note == NULL) {
        perror("MT_MALLOC");
        return 1;
    }
    memcpy(note, message, sizeof(message));
    puts(note);

    if (!leak) {
        MT_FREE(note);
    }
    mt_report(stdout);
    return mt_stats().active_blocks != 0;
}
