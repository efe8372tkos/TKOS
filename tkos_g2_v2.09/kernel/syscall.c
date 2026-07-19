#include "syscall.h"
#include "fb_console.h"
#include "heap.h"
#include "sched.h"
#include "task.h"
#include "pit.h"
#include "types.h"

/*
 * TKOS - Sistem Cagrilari Implementasyonu
 * Detayli aciklama icin syscall.h basina bakiniz.
 */

uint64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2,
                           uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a2; (void)a3; (void)a4; (void)a5; /* cogu syscall'da kullanilmiyor */

    switch (num) {
        case SYS_EXIT:
            /* demo_task_1/2/3 (kernel_main.c) ile AYNI desen:
             * kendini kill isaretleyip sonsuza dek yield eder;
             * gercek temizlik (bellek geri verme) BASKA bir
             * gorevin sched_next() taramasi sirasinda yapilir. */
            task_kill(sched_get_current());
            for (;;) task_yield();
            return 0; /* erisilmez */

        case SYS_WRITE:
            con_print((const char *)(uintptr_t)a1);
            return 0;

        case SYS_PUTC:
            con_putchar((char)a1);
            return 0;

        case SYS_SLEEP:
            task_sleep(a1);
            return 0;

        case SYS_YIELD:
            task_yield();
            return 0;

        case SYS_GETPID: {
            task_t *cur = sched_get_current();
            return cur ? cur->task_num : 0;
        }

        case SYS_ALLOC:
            return (uint64_t)(uintptr_t)kmalloc(a1);

        case SYS_FREE:
            kfree((void *)(uintptr_t)a1);
            return 0;

        case SYS_UPTIME_MS:
            return pit_get_ticks();

        default:
            return (uint64_t)-1;
    }
}
