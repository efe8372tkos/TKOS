#pragma once
/*
 * TKOS - TKX Uygulama Gelistirme Syscall ABI
 * Kernel'deki syscall.h ile numaralar BIREBIR AYNI olmali.
 */

#define SYS_EXIT        0
#define SYS_WRITE       1
#define SYS_PUTC        2
#define SYS_SLEEP       3
#define SYS_YIELD       4
#define SYS_GETPID      5
#define SYS_ALLOC       6
#define SYS_FREE        7
#define SYS_UPTIME_MS   8

static inline unsigned long tkos_syscall(unsigned long num, unsigned long a1,
                                          unsigned long a2, unsigned long a3,
                                          unsigned long a4, unsigned long a5) {
    register unsigned long r8 __asm__("r8") = a5;
    unsigned long ret;
    __asm__ volatile ("int $0x80"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3), "c"(a4), "r"(r8)
        : "memory", "cc");
    return ret;
}

static inline void sys_exit(int code)            { tkos_syscall(SYS_EXIT, (unsigned long)code, 0,0,0,0); }
static inline void sys_write(const char *s)       { tkos_syscall(SYS_WRITE, (unsigned long)s, 0,0,0,0); }
static inline void sys_putc(char c)               { tkos_syscall(SYS_PUTC, (unsigned long)c, 0,0,0,0); }
static inline void sys_sleep_ms(unsigned long ms) { tkos_syscall(SYS_SLEEP, ms, 0,0,0,0); }
static inline void sys_yield(void)                { tkos_syscall(SYS_YIELD, 0,0,0,0,0); }
static inline unsigned long sys_getpid(void)      { return tkos_syscall(SYS_GETPID,0,0,0,0,0); }
static inline void *sys_alloc(unsigned long sz)   { return (void*)tkos_syscall(SYS_ALLOC, sz,0,0,0,0); }
static inline void sys_free(void *p)              { tkos_syscall(SYS_FREE, (unsigned long)p,0,0,0,0); }
static inline unsigned long sys_uptime_ms(void)   { return tkos_syscall(SYS_UPTIME_MS,0,0,0,0,0); }
