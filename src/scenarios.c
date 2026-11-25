#include "scenarios.h"
#include "memory_tracker.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char *title;
    char *body;
} Document;

const char *scenario_name(Scenario scenario)
{
    static const char *names[] = {"fixed", "leak", "crash", "hang"};
    return names[scenario];
}

static void document_destroy(Document *document)
{
    if (document != NULL) {
        MT_FREE(document->body);
        MT_FREE(document->title);
        MT_FREE(document);
    }
}

int scenario_run(Scenario scenario)
{
    const char title[] = "Example";
    const char body[] = "A small document used by the fault-injection tests.";

    Document *document = MT_MALLOC(sizeof(*document));
    if (document == NULL) {
        return TASK_OUT_OF_MEMORY;
    }
    *document = (Document){0};
    document->title = MT_MALLOC(sizeof(title));
    if (document->title == NULL) {
        document_destroy(document);
        return TASK_OUT_OF_MEMORY;
    }
    memcpy(document->title, title, sizeof(title));
    document->body = MT_MALLOC(sizeof(body));
    if (document->body == NULL) {
        if (scenario == SC_LEAK) {
            MT_FREE(document);
            return TASK_OUT_OF_MEMORY;
        }
        if (scenario == SC_CRASH) {
            raise(SIGSEGV);
            _exit(120);
        }
        if (scenario == SC_HANG) {
            for (;;) {
                pause();
            }
        }
        document_destroy(document);
        return TASK_OUT_OF_MEMORY;
    }
    memcpy(document->body, body, sizeof(body));
    int valid = strcmp(document->title, title) == 0 &&
                strcmp(document->body, body) == 0;
    document_destroy(document);
    return valid ? TASK_OK : TASK_BAD_RESULT;
}
