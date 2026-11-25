#ifndef SCENARIOS_H
#define SCENARIOS_H

typedef enum { SC_FIXED, SC_LEAK, SC_CRASH, SC_HANG } Scenario;
enum { TASK_OK = 0, TASK_OUT_OF_MEMORY = 1, TASK_BAD_RESULT = 2 };

const char *scenario_name(Scenario scenario);
int scenario_run(Scenario scenario);

#endif
