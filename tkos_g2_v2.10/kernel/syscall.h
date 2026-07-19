#pragma once
#include "types.h"


























#define SYS_EXIT        0   
#define SYS_WRITE       1   
#define SYS_PUTC        2   
#define SYS_SLEEP       3   
#define SYS_YIELD       4   
#define SYS_GETPID      5   
#define SYS_ALLOC       6   
#define SYS_FREE        7   
#define SYS_UPTIME_MS   8   

#define SYS_COUNT       9






uint64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2,
                           uint64_t a3, uint64_t a4, uint64_t a5);
