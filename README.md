# MemoryTracker

MemoryTracker is a library and command-line test bench for a question that is easy to miss in a passing test: if the third allocation fails, who frees the first two?

The library records where a program allocates memory, how many bytes are still in use, and how high that total reached. It can also make one selected allocation return `NULL`. The test bench uses these features to run four versions of the same document-building task in separate Linux processes. It reports whether each version cleaned up, leaked memory, crashed, or exceeded its time limit.

## Quick start

Use Linux or a Linux terminal in WSL. The build uses a C compiler available as `cc`, GNU Make, and `ar`. The integration tests also use the Linux `timeout` utility from GNU coreutils. The program, example, and tests are written in C.

```sh
git clone https://github.com/scztr/MemoryTracker.git
cd MemoryTracker
make
./memorytracker fixed
```

The first row is a normal run. The next three runs each fail one allocation. Here is the result on 64-bit Linux under WSL:

```text
Scenario fail_at Verdict        Attempts  Blocks   Bytes    Peak  Task
fixed         0 PASS                  3       0       0      76     0
fixed         1 PASS                  1       0       0       0     1
fixed         2 PASS                  2       0       0      16     1
fixed         3 PASS                  3       0       0      24     1
Summary: 4 runs, 4 passed, 0 problems
```

`Task = 1` and `PASS` belong together here. The task reported that it ran out of memory. The test passed because the failure was intentional, the task returned the expected result, and it released every block it had acquired.

## The document experiment

A document owns two strings. On a successful run, the scenario makes three tracked allocations in order:

| Attempt | Allocation | What the task already owns if this attempt fails |
| --- | --- | --- |
| 1 | The `Document` structure | Nothing |
| 2 | A copy of the title, `"Example"` | The structure |
| 3 | A copy of the body text | The structure and the title |

Compare two implementations at the same failure point:

```sh
./memorytracker fixed 3
./memorytracker leak 3
```

Selected lines from the two runs:

```text
Scenario fail_at Verdict        Attempts  Blocks   Bytes    Peak  Task
fixed         3 PASS                  3       0       0      24     1
leak          3 LEAK                  3       1       8      24     1
  First outstanding block: src/scenarios.c:38 (8 bytes)
```

Both versions return the same task result. The difference is a single cleanup decision. The correct version calls `document_destroy(document)`, which frees the owned strings and then their containing structure. The leaking version calls only `MT_FREE(document)`. Freeing a structure does not free the heap objects its pointers refer to. The eight bytes are the seven characters in `"Example"` plus its terminating zero. The reported line points to the allocation that created the title.

### Crashes and timeouts

```sh
./memorytracker crash 3
./memorytracker hang 3
```

The crash scenario deliberately raises `SIGSEGV`. The hang scenario waits indefinitely. The supervisor can report both outcomes because it runs outside the child process:

```text
crash         3 CRASH                 -       -       -       -     -
  Child terminated by signal 11; memory report unavailable.
hang          3 TIMEOUT               -       -       -       -     -
  Child stopped after 500 ms; memory report unavailable.
```

A dash means the child did not return a final memory report. Its outstanding byte count is unknown.

## Command reference

```text
./memorytracker [all|fixed|leak|crash|hang] [fail_at]
```

| Command | What it runs |
| --- | --- |
| `./memorytracker` or `./memorytracker all` | All four scenarios, each with its own baseline and failure sweep |
| `./memorytracker fixed` | A baseline followed by one run for each allocation attempt observed in that baseline |
| `./memorytracker fixed 0` | A single baseline run |
| `./memorytracker leak 3` | A single run with the third allocation forced to fail |
| `./memorytracker all 3` | The third-allocation failure once in each scenario |
| `./memorytracker fixed 4` | An example of `NOT_REACHED`: this task makes only three attempts |

The CLI accepts failure indices from 0 through 10000. When the index is omitted, only scenarios with a passing baseline proceed to a sweep.

### Reading the report

| Column | Meaning |
| --- | --- |
| `fail_at` | Index of the allocation selected for failure; zero disables injection |
| `Attempts` | Tracked allocation attempts, including an injected failure |
| `Blocks` / `Bytes` | Number and total size of blocks still allocated when the task returned |
| `Peak` | Largest number of tracked bytes held at the same time during this run |
| `Task` | Result returned by the scenario: 0 for success, 1 for handled allocation failure, 2 for a bad task result |

| Verdict | Meaning |
| --- | --- |
| `PASS` | The task returned the expected result, handled the requested injection if any, and released all tracked blocks. |
| `LEAK` | The task returned with tracked blocks still allocated. The CLI shows their total and one allocation site. |
| `CRASH` | A signal terminated the child. |
| `TIMEOUT` | The child exceeded the 500 ms limit. |
| `NOT_REACHED` | The scenario finished before reaching the selected failure index. |
| `WRONG_RESULT` | The task returned an unexpected result after the other checks passed. |
| `TRACKER_ERROR` | The tracker recorded an invalid operation, a capacity or size error, or an actual system allocation failure. |
| `HARNESS_ERROR` | The supervisor could not complete the run or obtain a valid report. |

**Shell exit codes:** 0 when all requested runs pass, 1 when at least one run reports a problem, and 2 for invalid arguments. These codes describe the overall CLI invocation. The `Task` column describes one scenario.

## Use the library in your own program

The static library works independently of the document scenarios. Include `memory_tracker.h`, use `MT_MALLOC` and `MT_FREE` for the allocations you want to track, and call `mt_report` at a point where you know which objects should still be alive.

### Try the standalone example

```sh
make example
./examples/notes/notes
./examples/notes/notes --leak
```

Both runs print the same note. The difference appears in the report:

```text
$ ./examples/notes/notes
Study C
Active: 0 blocks, 0 bytes; peak: 8 bytes

$ ./examples/notes/notes --leak
Study C
notes.c:15: 8 bytes still allocated
Active: 1 blocks, 8 bytes; peak: 8 bytes
```

The example has its own [source file](examples/notes/notes.c) and [Makefile](examples/notes/Makefile). Its exit code is 1 when the note is left allocated.

### Track an array of grades

Save the following as `grades.c` in the repository root:

```c
#include "memory_tracker.h"
#include <stdio.h>

int main(void)
{
    int *grades = MT_MALLOC(10 * sizeof *grades);
    if (grades == NULL) {
        perror("Could not allocate grades");
        return 1;
    }

    for (int i = 0; i < 10; ++i) {
        grades[i] = 80 + i;
    }
    printf("First grade: %d, last grade: %d\n", grades[0], grades[9]);

    MT_FREE(grades);
    mt_report(stdout);
    return 0;
}
```

```sh
make library
cc -std=c11 -Iinclude grades.c -Llib -lmemtracker -o grades
./grades
```

On a system with four-byte integers, the report shows a peak of 40 bytes and zero active blocks. Remove the `MT_FREE(grades)` line and rebuild the example to see a 40-byte outstanding allocation. To test the allocation-error branch instead, insert `mt_reset(1);` immediately before `MT_MALLOC` in this fresh process. The tracker returns `NULL` on that attempt and sets `errno` to `ENOMEM`.

### Public API

The declarations and statistics structure live in [memory_tracker.h](include/memory_tracker.h).

| Call | Behavior |
| --- | --- |
| `MT_MALLOC(size)` | Allocate a block and record its size, file, and line. The macro supplies the caller's `__FILE__` and `__LINE__`. |
| `MT_FREE(ptr)` | Release a tracked block and remove its entry. Passing `NULL` is safe. An unknown address increments the error counter. |
| `mt_reset(fail_at)` | Clear statistics and select the next run's failure index. Returns −1 if any tracked block is still active. |
| `mt_stats()` | Return an `MtStats` snapshot, including attempts, successful allocations, frees, live and peak bytes, and injected or system failures. |
| `mt_first_active(&file, &line, &size)` | Return 1 and fill in the details of one outstanding block, or return 0 when none remain. |
| `mt_report(stream)` | Write every outstanding allocation and the totals to a `FILE *`, such as `stdout` or an open log file. |

## How the supervisor works

1. The parent creates a pipe and calls `fork()`. The child starts with fresh tracking statistics and its selected failure index.
2. The child runs the scenario. If it returns, the child sends its task result, counters, and one outstanding allocation site through the pipe.
3. The parent checks the child with `waitpid()` and measures the time limit using a monotonic clock. If the child exceeds the limit, the parent stops it and collects its exit status.
4. The parent assigns a verdict. In a sweep, it repeats the experiment in a fresh process for each allocation index seen in the successful baseline.

## Tests

```sh
make test
```

The final test messages are:

```text
Tracker tests: PASS
Supervisor and standalone example tests: PASS
```

[Tracker tests](tests/test_tracker.c) exercise accounting, peak usage, allocation-site reporting, a selected failure, invalid frees, and the capacity limit. [CLI tests](tests/test_cli.c) check the full sweep, single-run verdicts, invalid arguments, exit codes, and both modes of the standalone example.

Running `./memorytracker all` directly produces `Summary: 16 runs, 13 passed, 3 problems`. Those three problems are the intentional leak, crash, and hang. The integration test passes when the bench identifies each one correctly.

## Project layout

| Path | Purpose |
| --- | --- |
| [src/memory_tracker.c](src/memory_tracker.c) | Allocation table, fault injection, statistics, and reports |
| [include/memory_tracker.h](include/memory_tracker.h) | Public library interface |
| [src/main.c](src/main.c) | CLI, child-process supervisor, timeout handling, and verdicts |
| [src/scenarios.c](src/scenarios.c) | Document construction, cleanup, and the four demonstration behaviors |
| [examples/notes](examples/notes) | Separate application linked against the library |
| [tests](tests) | Library and command-line tests in C |
| [Makefile](Makefile) | Builds the program, static library, example, and tests |

## Scope and limits

- The library tracks allocations made through its wrappers. Direct calls to `malloc`, `calloc`, and `realloc` are outside its accounting. Pair each tracked allocation with `MT_FREE`.
- The tracker is single-threaded and holds up to 128 simultaneously active blocks. Freed entries become available again. A zero-byte request is rejected.
- An active block means the caller still owns it at the moment of the report. Check at the end of the operation whose cleanup you want to verify.
- The CLI runs the four built-in scenarios. An external C application can link the library, select failure indices, and request its own reports. The public library API currently covers tracking and injection; the process supervisor belongs to the CLI.
- Peak byte counts can vary with the platform's structure layout. Source line numbers can change as the examples are edited.
