#pragma once
#include "types.h"













#define TASK_NAME_LEN       32
#define TASK_SIGNATURE_VAL  0x546B5353UL    

#define TASK_STK_SIZE       (4096 * 8)      
#define TASK_MAX            64              





#define TASKf_TASK_LOCK     0   
#define TASKf_KILL_TASK     1   
#define TASKf_SUSPENDED     2   
#define TASKf_IDLE          3   
#define TASKf_AWAITING_MSG  9   


#define TASK_FLAG_SET(t, f)   ((t)->task_flags |=  (1U << (f)))
#define TASK_FLAG_CLR(t, f)   ((t)->task_flags &= ~(1U << (f)))
#define TASK_FLAG_TEST(t, f)  (((t)->task_flags >> (f)) & 1U)





typedef struct {
    uint8_t body[512];
} __attribute__((aligned(16))) fpu_state_t;













typedef struct task {
    
    struct task *addr;              
    uint32_t    task_signature;     
    uint32_t    task_flags;         

    
    char        task_name[TASK_NAME_LEN];
    uint32_t    task_num;           

    









    uint64_t    rip;
    uint64_t    rflags;
    uint64_t    rsp;
    uint64_t    rsi;
    uint64_t    rax;
    uint64_t    rcx;
    uint64_t    rdx;
    uint64_t    rbx;
    uint64_t    rbp;
    uint64_t    rdi;
    uint64_t    r8;
    uint64_t    r9;
    uint64_t    r10;
    uint64_t    r11;
    uint64_t    r12;
    uint64_t    r13;
    uint64_t    r14;
    uint64_t    r15;

    
    fpu_state_t *fpu_state;         

    
    uint64_t    wake_jiffy;         
    uint64_t    total_jiffies;      
    uint64_t    swap_cnter;         

    
    struct task *next_task;
    struct task *last_task;

    
    uint8_t     *stk_base;          
    uint64_t    stk_size;           

    void        *image_base;
    uint64_t    image_pages;

    
    uint64_t    user_data;
} task_t;







typedef struct {
    
    void       *addr;

    
    uint64_t    num;

    
    uint64_t    total_jiffies;
    uint64_t    idle_pt_hits;

    
    task_t     *current_task;       
    task_t     *idle_task;          

    
    uint64_t    swap_cnter;
} cpu_t;






extern cpu_t cpu0;
extern task_t *current_task;











task_t *task_create(const char *name,
                    void (*entry)(void),
                    uint64_t stk_size);





void task_kill(task_t *t);






void task_sleep(uint64_t jiffies);





void task_yield(void);
