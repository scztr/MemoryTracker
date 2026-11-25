#include "memory_tracker.h"
#include "scenarios.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TIMEOUT_MS 500
#define MAX_FAILURE_INDEX 10000
#define REPORT_MAGIC 0x4d545250u

typedef struct {
    unsigned int magic;
    int task_result;
    MtStats stats;
    char first_file[80];
    int first_line;
    size_t first_size;
} ChildReport;

_Static_assert(sizeof(ChildReport) <= _POSIX_PIPE_BUF, "Report is too large");

typedef enum { RUN_DONE, RUN_CRASH, RUN_TIMEOUT, RUN_ERROR } RunState;
typedef struct {
    RunState state;
    int signal_number;
    ChildReport report;
} Run;

static int64_t milliseconds(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return -1;
    }
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int write_report(int fd, const ChildReport *report)
{
    ssize_t written;
    do {
        written = write(fd, report, sizeof(*report));
    } while (written < 0 && errno == EINTR);
    return written == (ssize_t)sizeof(*report) ? 0 : -1;
}

static int read_report(int fd, ChildReport *report)
{
    size_t received = 0;
    while (received < sizeof(*report)) {
        ssize_t n = read(fd, (char *)report + received, sizeof(*report) - received);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n <= 0) {
            return -1;
        }
        received += (size_t)n;
    }
    return report->magic == REPORT_MAGIC ? 0 : -1;
}

static void child_run(int fd, Scenario scenario, size_t fail_at)
{
    struct rlimit no_core = {0, 0};
    if (setrlimit(RLIMIT_CORE, &no_core) != 0 ||
        signal(SIGSEGV, SIG_DFL) == SIG_ERR || mt_reset(fail_at) != 0) {
        _exit(121);
    }
    ChildReport report = {0};
    report.magic = REPORT_MAGIC;
    report.task_result = scenario_run(scenario);
    report.stats = mt_stats();
    const char *file;
    if (mt_first_active(&file, &report.first_line, &report.first_size)) {
        snprintf(report.first_file, sizeof(report.first_file), "%s", file);
    }
    int failed = write_report(fd, &report);
    close(fd);
    _exit(failed == 0 ? 0 : 122);
}

static void stop_child(pid_t pid)
{
    (void)kill(pid, SIGKILL);
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
    }
}

static Run run_test(Scenario scenario, size_t fail_at)
{
    Run run = {.state = RUN_ERROR};
    int channel[2];
    if (pipe(channel) != 0) {
        perror("pipe");
        return run;
    }
    int64_t start = milliseconds();
    if (start < 0) {
        perror("clock_gettime");
        close(channel[0]);
        close(channel[1]);
        return run;
    }
    fflush(NULL);
    pid_t pid = fork();
    if (pid == 0) {
        close(channel[0]);
        child_run(channel[1], scenario, fail_at);
    }
    close(channel[1]);
    if (pid < 0) {
        perror("fork");
        close(channel[0]);
        return run;
    }
    int status = 0;
    for (;;) {
        pid_t completed = waitpid(pid, &status, WNOHANG);
        if (completed == pid) {
            break;
        }
        if (completed < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("waitpid");
            close(channel[0]);
            return run;
        }
        int64_t now = milliseconds();
        if (now < 0 || now - start >= TIMEOUT_MS) {
            stop_child(pid);
            run.state = now < 0 ? RUN_ERROR : RUN_TIMEOUT;
            close(channel[0]);
            return run;
        }
        struct timespec delay = {0, 10000000};
        (void)nanosleep(&delay, NULL);
    }
    if (WIFSIGNALED(status)) {
        run.state = RUN_CRASH;
        run.signal_number = WTERMSIG(status);
    } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
               read_report(channel[0], &run.report) == 0) {
        run.state = RUN_DONE;
    }
    close(channel[0]);
    return run;
}

static const char *verdict(const Run *run, size_t fail_at)
{
    if (run->state == RUN_CRASH) return "CRASH";
    if (run->state == RUN_TIMEOUT) return "TIMEOUT";
    if (run->state == RUN_ERROR) return "HARNESS_ERROR";
    const MtStats *s = &run->report.stats;
    if (s->errors || s->system_failures) return "TRACKER_ERROR";
    if (fail_at && s->injected_failures != 1) return "NOT_REACHED";
    if (s->active_blocks) return "LEAK";
    int expected = fail_at ? TASK_OUT_OF_MEMORY : TASK_OK;
    if (run->report.task_result != expected) return "WRONG_RESULT";
    return "PASS";
}

static int display(Scenario scenario, size_t fail_at, const Run *run)
{
    const char *result = verdict(run, fail_at);
    printf("%-7s %7zu %-14s", scenario_name(scenario), fail_at, result);
    if (run->state == RUN_DONE) {
        const MtStats *s = &run->report.stats;
        printf(" %8zu %7zu %7zu %7zu %5d\n", s->attempts, s->active_blocks,
               s->active_bytes, s->peak_bytes, run->report.task_result);
        if (s->active_blocks) {
            printf("  First outstanding block: %s:%d (%zu bytes)\n",
                   run->report.first_file, run->report.first_line,
                   run->report.first_size);
        }
    } else {
        printf("        -       -       -       -     -\n");
        if (run->state == RUN_CRASH) {
            printf("  Child terminated by signal %d; memory report unavailable.\n",
                   run->signal_number);
        } else if (run->state == RUN_TIMEOUT) {
            printf("  Child stopped after %d ms; memory report unavailable.\n", TIMEOUT_MS);
        }
    }
    return strcmp(result, "PASS") != 0;
}

int main(int argc, char **argv)
{
    int first = SC_FIXED, last = SC_HANG;
    size_t selected_failure = 0;
    if (argc > 3) goto usage;
    if (argc >= 2 && strcmp(argv[1], "all") != 0) {
        int found = 0;
        for (int i = SC_FIXED; i <= SC_HANG; i++) {
            if (strcmp(argv[1], scenario_name((Scenario)i)) == 0) {
                first = last = i;
                found = 1;
                break;
            }
        }
        if (!found) goto usage;
    }
    if (argc == 3) {
        if (argv[2][0] < '0' || argv[2][0] > '9') goto usage;
        char *end;
        errno = 0;
        unsigned long number = strtoul(argv[2], &end, 10);
        if (errno || *end || number > MAX_FAILURE_INDEX) goto usage;
        selected_failure = (size_t)number;
    }
    size_t total = 0, problems = 0;
    puts("MemoryTracker: allocation-failure test bench");
    puts("fail_at=0: baseline; task result 0: success, 1: handled allocation failure");
    puts("Scenario fail_at Verdict        Attempts  Blocks   Bytes    Peak  Task");
    for (int i = first; i <= last; i++) {
        Scenario scenario = (Scenario)i;
        size_t initial = argc == 3 ? selected_failure : 0;
        Run baseline = run_test(scenario, initial);
        int bad = display(scenario, initial, &baseline);
        total++;
        problems += (size_t)bad;
        if (argc == 3 || bad) continue;
        size_t attempts = baseline.report.stats.attempts;
        for (size_t fail_at = 1; fail_at <= attempts; fail_at++) {
            Run run = run_test(scenario, fail_at);
            problems += (size_t)display(scenario, fail_at, &run);
            total++;
        }
    }
    printf("Summary: %zu runs, %zu passed, %zu problems\n", total, total - problems, problems);
    return problems ? EXIT_FAILURE : EXIT_SUCCESS;

usage:
    fprintf(stderr, "Usage: %s [all|fixed|leak|crash|hang] [fail_at: 0..%d]\n",
            argv[0], MAX_FAILURE_INDEX);
    return 2;
}
