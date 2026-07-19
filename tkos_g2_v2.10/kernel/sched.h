#pragma once
#include "task.h"
#include "types.h"
















#define SCHED_TICK_SLICE    1       






void sched_init(void);






void sched_add(task_t *t);





void sched_remove(task_t *t);









void sched_tick(void);






void sched_yield(void);





task_t *sched_get_current(void);




uint32_t sched_get_task_count(void);




void sched_print_tasks(void);











extern void sched_context_switch(task_t *prev, task_t *next);
