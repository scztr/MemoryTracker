#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static char output[8192];
static char command[256];

static void require(int ok, const char *reason)
{
    if (!ok) {
        fprintf(stderr, "FAIL: %s\nCommand: %s\n%s", reason, command, output);
        exit(EXIT_FAILURE);
    }
}

static void run(const char *args, int expected)
{
    snprintf(command, sizeof(command), "timeout -k 1s 8s %s 2>&1", args);
    output[0] = '\0';
    FILE *stream = popen(command, "r");
    require(stream != NULL, "could not start command");
    size_t bytes = fread(output, 1, sizeof(output) - 1, stream);
    output[bytes] = '\0';
    require(feof(stream) && !ferror(stream), "incomplete command output");
    int status = pclose(stream);
    require(status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == expected,
            "unexpected exit status");
}

static void contains(const char *expected)
{
    require(strstr(output, expected) != NULL, expected);
}

static void test_sweep(void)
{
    const char *names[] = {"fixed", "leak", "crash", "hang"};
    const char *last[] = {"PASS", "LEAK", "CRASH", "TIMEOUT"};
    run("./memorytracker all", 1);
    contains("Summary: 16 runs, 13 passed, 3 problems");
    contains("First outstanding block: src/scenarios.c:");
    contains("(8 bytes)");

    int rows = 0, missing = 0;
    for (char *line = strtok(output, "\n"); line; line = strtok(NULL, "\n")) {
        if (strstr(line, "memory report unavailable")) ++missing;
        char name[16], verdict[24];
        int index;
        if (sscanf(line, "%15s %d %23s", name, &index, verdict) != 3) continue;
        if (strcmp(name, "Summary:") == 0) continue;
        require(rows < 16, "too many scenario rows");
        int scenario = rows / 4;
        require(strcmp(name, names[scenario]) == 0 && index == rows % 4,
                "unexpected scenario or failure index");
        require(strcmp(verdict, index == 3 ? last[scenario] : "PASS") == 0,
                "wrong verdict");
        ++rows;
    }
    require(rows == 16 && missing == 2, "incomplete sweep");
}

int main(void)
{
    test_sweep();
    const struct { const char *args; int code; const char *message; } cases[] = {
        {"./memorytracker fixed", 0, "Summary: 4 runs, 4 passed, 0 problems"},
        {"./memorytracker fixed 3", 0, "Summary: 1 runs, 1 passed, 0 problems"},
        {"./memorytracker leak 0", 0, "Summary: 1 runs, 1 passed, 0 problems"},
        {"./memorytracker leak 3", 1, "LEAK"},
        {"./memorytracker crash 3", 1, "CRASH"},
        {"./memorytracker hang 3", 1, "TIMEOUT"},
        {"./memorytracker fixed 4", 1, "NOT_REACHED"},
        {"./memorytracker unknown", 2, "Usage:"},
        {"./memorytracker fixed -1", 2, "Usage:"},
        {"./memorytracker fixed abc", 2, "Usage:"},
        {"./memorytracker fixed ''", 2, "Usage:"},
        {"./memorytracker fixed 10001", 2, "Usage:"},
        {"./memorytracker fixed 1 extra", 2, "Usage:"}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        run(cases[i].args, cases[i].code);
        contains(cases[i].message);
    }
    run("./examples/notes/notes", 0);
    require(strcmp(output, "Study C\nActive: 0 blocks, 0 bytes; peak: 8 bytes\n") == 0,
            "unexpected clean report");
    run("./examples/notes/notes --leak", 1);
    int line = 0;
    require(sscanf(output, "Study C\nnotes.c:%d", &line) == 1 && line > 0,
            "missing allocation location");
    contains(": 8 bytes still allocated\n");
    contains("Active: 1 blocks, 8 bytes; peak: 8 bytes");
    puts("Supervisor and standalone example tests: PASS");
    return 0;
}
