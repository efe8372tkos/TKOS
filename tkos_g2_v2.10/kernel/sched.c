#include "sched.h"
#include "task.h"
#include "alloc.h"
#include "pit.h"
#include "string.h"
#include "fb_console.h"
#include "types.h"










cpu_t  cpu0;
task_t *current_task = NULL;





static task_t  *task_list_head = NULL;  
static uint32_t task_count     = 0;    
static uint32_t next_task_num  = 1;    
static uint64_t tick_counter   = 0;    






static task_t idle_task_struct;
static uint8_t idle_task_stack[4096];

static void idle_task_fn(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}







static void list_add(task_t *t) {
    if (!task_list_head) {
        
        t->next_task = t;
        t->last_task = t;
        task_list_head = t;
    } else {
        
        task_t *tail = task_list_head->last_task;
        tail->next_task = t;
        t->last_task    = tail;
        t->next_task    = task_list_head;
        task_list_head->last_task = t;
    }
    task_count++;
}


static void list_remove(task_t *t) {
    if (!t || task_count == 0) return;

    if (t->next_task == t) {
        
        task_list_head = NULL;
    } else {
        t->last_task->next_task = t->next_task;
        t->next_task->last_task = t->last_task;
        if (task_list_head == t)
            task_list_head = t->next_task;
    }

    t->next_task = NULL;
    t->last_task = NULL;
    task_count--;
}







void sched_init(void) {
    
    cpu0.addr          = &cpu0;
    cpu0.num           = 0;
    cpu0.total_jiffies = 0;
    cpu0.idle_pt_hits  = 0;
    cpu0.swap_cnter    = 0;

    
    memset(&idle_task_struct, 0, sizeof(task_t));
    idle_task_struct.addr           = &idle_task_struct;
    idle_task_struct.task_signature = TASK_SIGNATURE_VAL;
    idle_task_struct.task_num       = 0;
    idle_task_struct.stk_base       = idle_task_stack;
    idle_task_struct.stk_size       = sizeof(idle_task_stack);

    
    uint64_t stk_top = (uint64_t)(uintptr_t)(idle_task_stack +
                        sizeof(idle_task_stack));
    stk_top &= ~0xFULL; 

    idle_task_struct.rip    = (uint64_t)(uintptr_t)idle_task_fn;
    idle_task_struct.rsp    = stk_top;
    idle_task_struct.rflags = 0x202; 

    memcpy(idle_task_struct.task_name, "Idle", 5);
    TASK_FLAG_SET(&idle_task_struct, TASKf_IDLE);

    cpu0.idle_task    = &idle_task_struct;
    cpu0.current_task = &idle_task_struct;
    current_task      = &idle_task_struct;

    
    list_add(&idle_task_struct);
}

static void task_auto_exit(void) {
    task_kill(sched_get_current());
    for (;;) task_yield();
}







task_t *task_create(const char *name,
                    void (*entry)(void),
                    uint64_t stk_size) {
    if (!entry) return NULL;
    if (stk_size == 0) stk_size = TASK_STK_SIZE;

    
    uint64_t stk_pages  = (stk_size + 4095) / 4096;
    uint64_t task_pages = (sizeof(task_t) + 4095) / 4096;

    
    task_t  *t   = (task_t *)alloc_pages(task_pages);
    uint8_t *stk = (uint8_t *)alloc_pages(stk_pages);

    if (!t || !stk) {
        if (t)   free_pages(t,   task_pages);
        if (stk) free_pages(stk, stk_pages);
        return NULL;
    }

    
    t->addr           = t;
    t->task_signature = TASK_SIGNATURE_VAL;
    t->task_num       = next_task_num++;
    t->stk_base       = stk;
    t->stk_size       = stk_pages * 4096;
    t->wake_jiffy     = 0;
    t->total_jiffies  = 0;
    t->swap_cnter     = 0;

    
    uint32_t i;
    for (i = 0; i < TASK_NAME_LEN - 1 && name && name[i]; i++)
        t->task_name[i] = name[i];
    t->task_name[i] = '\0';

    











    uint64_t stk_top = (uint64_t)(uintptr_t)(stk + stk_pages * 4096);
    stk_top &= ~0xFULL;         
    stk_top -= 8;               
      *(uint64_t *)(uintptr_t)stk_top = (uint64_t)(uintptr_t)task_auto_exit;
		    
    t->rip    = (uint64_t)(uintptr_t)entry;
    t->rsp    = stk_top;
    t->rflags = 0x202;  
    t->rbp    = 0;

    return t;
}





void sched_add(task_t *t) {
    if (!t) return;
    list_add(t);
}





void sched_remove(task_t *t) {
    if (!t) return;
    list_remove(t);
}





void task_kill(task_t *t) {
    if (!t) return;
    TASK_FLAG_SET(t, TASKf_KILL_TASK);
}








void task_sleep(uint64_t jiffies) {
    if (!current_task) return;
    current_task->wake_jiffy = cpu0.total_jiffies + jiffies;
    TASK_FLAG_SET(current_task, TASKf_SUSPENDED);
    sched_yield();
}





void task_yield(void) {
    sched_yield();
}












static task_t *sched_next(void) {
    if (!task_list_head) return cpu0.idle_task;

    task_t *start = current_task->next_task;
    task_t *t     = start;

    do {
        
        if (TASK_FLAG_TEST(t, TASKf_KILL_TASK) &&
            t != cpu0.idle_task) {
            task_t *victim = t;
            t = t->next_task;
            list_remove(victim);
            
            uint64_t stk_pages  = victim->stk_size / 4096;
            uint64_t task_pages = (sizeof(task_t) + 4095) / 4096;

            if (victim->image_base) {
                free_pages(victim->image_base, victim->image_pages);
            }

            free_pages(victim->stk_base, stk_pages);
            free_pages(victim, task_pages);
            continue;
        }

        
        if (TASK_FLAG_TEST(t, TASKf_SUSPENDED)) {
            if (cpu0.total_jiffies >= t->wake_jiffy) {
                TASK_FLAG_CLR(t, TASKf_SUSPENDED);
                t->wake_jiffy = 0;
            } else {
                t = t->next_task;
                continue;
            }
        }

        
        return t;

    } while (t != start);

    
    return cpu0.idle_task;
}









void sched_tick(void) {
    
    cpu0.total_jiffies++;

    
    if (current_task && TASK_FLAG_TEST(current_task, TASKf_IDLE))
        cpu0.idle_pt_hits++;

    
    if (tick_counter > 0) {
        tick_counter--;
        return;
    }

    
    tick_counter = SCHED_TICK_SLICE;

    task_t *next = sched_next();
    if (!next || next == current_task) return;

    
    task_t *prev  = current_task;
    current_task  = next;
    cpu0.current_task = next;
    cpu0.swap_cnter++;
    next->swap_cnter++;

    sched_context_switch(prev, next);
}





void sched_yield(void) {
    task_t *next = sched_next();
    if (!next || next == current_task) return;

    task_t *prev  = current_task;
    current_task  = next;
    cpu0.current_task = next;
    cpu0.swap_cnter++;
    next->swap_cnter++;

    sched_context_switch(prev, next);
}




task_t *sched_get_current(void) {
    return current_task;
}




uint32_t sched_get_task_count(void) {
    return task_count;
}





void sched_print_tasks(void) {
    con_print("[SCHED] Tasks: ");
    con_print_dec(task_count);
    con_print(" | Jiffies: ");
    con_print_dec(cpu0.total_jiffies);
    con_print(" | Swaps: ");
    con_print_dec(cpu0.swap_cnter);
    con_print("\n");

    if (!task_list_head) return;

    task_t *t = task_list_head;
    do {
        con_print("  [");
        con_print_dec(t->task_num);
        con_print("] ");
        con_print(t->task_name);
        if (t == current_task) con_print(" <-- current");
        if (TASK_FLAG_TEST(t, TASKf_IDLE))      con_print(" [idle]");
        if (TASK_FLAG_TEST(t, TASKf_SUSPENDED)) con_print(" [sleep]");
        if (TASK_FLAG_TEST(t, TASKf_KILL_TASK)) con_print(" [dying]");
        con_print("\n");
        t = t->next_task;
    } while (t != task_list_head);
}
