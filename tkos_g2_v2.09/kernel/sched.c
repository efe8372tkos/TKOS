#include "sched.h"
#include "task.h"
#include "alloc.h"
#include "pit.h"
#include "string.h"
#include "fb_console.h"
#include "types.h"

/*
 * TKOS - Round-Robin Zamanlayici Implementasyonu
 * TempleOS Sched.HC ve MultiProc.HC'den uyarlanmistir.
 */

/* ------------------------------------------------
 * Global CPU yapisi (tek cekirdek)
 * TempleOS: GS segment -> CCPU
 * ------------------------------------------------ */
cpu_t  cpu0;
task_t *current_task = NULL;

/* ------------------------------------------------
 * Gorev listesi - dairesel cift bagli
 * TempleOS next_task / last_task zinciri
 * ------------------------------------------------ */
static task_t  *task_list_head = NULL;  /* Liste basi */
static uint32_t task_count     = 0;    /* Aktif gorev sayisi */
static uint32_t next_task_num  = 1;    /* Benzersiz ID sayaci */
static uint64_t tick_counter   = 0;    /* Mevcut dilim sayaci */

/* ------------------------------------------------
 * Bos dongu gorevi
 * TempleOS CCPU.idle_task + TASKf_IDLE karsiligi.
 * CPU'da baska gorev yokken calisir.
 * ------------------------------------------------ */
static task_t idle_task_struct;
static uint8_t idle_task_stack[4096];

static void idle_task_fn(void) {
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

/* ------------------------------------------------
 * Gorev listesi yardimcilari
 * TempleOS QueIns / QueRem mantigindan uyarlanmistir.
 * ------------------------------------------------ */

/* Gorevi dairesel listeye ekle (kuyruk sonu) */
static void list_add(task_t *t) {
    if (!task_list_head) {
        /* Tek eleman: kendine isaret eder */
        t->next_task = t;
        t->last_task = t;
        task_list_head = t;
    } else {
        /* Listenin sonuna ekle */
        task_t *tail = task_list_head->last_task;
        tail->next_task = t;
        t->last_task    = tail;
        t->next_task    = task_list_head;
        task_list_head->last_task = t;
    }
    task_count++;
}

/* Gorevi listeden cikar */
static void list_remove(task_t *t) {
    if (!t || task_count == 0) return;

    if (t->next_task == t) {
        /* Son eleman */
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

/* ------------------------------------------------
 * sched_init()
 * TempleOS Core0Init -> CPUStructInit mantigiyla:
 *   - CPU0 yapisini baslat
 *   - Bos dongu gorevini olustur ve ekle
 * ------------------------------------------------ */
void sched_init(void) {
    /* CPU0 baslat */
    cpu0.addr          = &cpu0;
    cpu0.num           = 0;
    cpu0.total_jiffies = 0;
    cpu0.idle_pt_hits  = 0;
    cpu0.swap_cnter    = 0;

    /* Bos dongu gorevini elle kur (heap gerektirmez) */
    memset(&idle_task_struct, 0, sizeof(task_t));
    idle_task_struct.addr           = &idle_task_struct;
    idle_task_struct.task_signature = TASK_SIGNATURE_VAL;
    idle_task_struct.task_num       = 0;
    idle_task_struct.stk_base       = idle_task_stack;
    idle_task_struct.stk_size       = sizeof(idle_task_stack);

    /* Yigin hazirla: ust noktadan asagi */
    uint64_t stk_top = (uint64_t)(uintptr_t)(idle_task_stack +
                        sizeof(idle_task_stack));
    stk_top &= ~0xFULL; /* 16-byte hizala */

    idle_task_struct.rip    = (uint64_t)(uintptr_t)idle_task_fn;
    idle_task_struct.rsp    = stk_top;
    idle_task_struct.rflags = 0x202; /* IF=1, rezerv=1 */

    memcpy(idle_task_struct.task_name, "Idle", 5);
    TASK_FLAG_SET(&idle_task_struct, TASKf_IDLE);

    cpu0.idle_task    = &idle_task_struct;
    cpu0.current_task = &idle_task_struct;
    current_task      = &idle_task_struct;

    /* Bos dongu gorevini listeye ekle */
    list_add(&idle_task_struct);
}

static void task_auto_exit(void) {
    task_kill(sched_get_current());
    for (;;) task_yield();
}
/* ------------------------------------------------
 * task_create()
 * TempleOS Spawn() mantigindan uyarlanmistir:
 *   - CAlloc ile gorev yapisi tahsis et
 *   - Yigin hazirla
 *   - Register'lari baslat
 * ------------------------------------------------ */
task_t *task_create(const char *name,
                    void (*entry)(void),
                    uint64_t stk_size) {
    if (!entry) return NULL;
    if (stk_size == 0) stk_size = TASK_STK_SIZE;

    /* Sayfa sayisini hesapla, yukarı yuvarla */
    uint64_t stk_pages  = (stk_size + 4095) / 4096;
    uint64_t task_pages = (sizeof(task_t) + 4095) / 4096;

    /* CAlloc karsiligi: sifirlanmis tahsis */
    task_t  *t   = (task_t *)alloc_pages(task_pages);
    uint8_t *stk = (uint8_t *)alloc_pages(stk_pages);

    if (!t || !stk) {
        if (t)   free_pages(t,   task_pages);
        if (stk) free_pages(stk, stk_pages);
        return NULL;
    }

    /* Gorev yapisini doldur */
    t->addr           = t;
    t->task_signature = TASK_SIGNATURE_VAL;
    t->task_num       = next_task_num++;
    t->stk_base       = stk;
    t->stk_size       = stk_pages * 4096;
    t->wake_jiffy     = 0;
    t->total_jiffies  = 0;
    t->swap_cnter     = 0;

    /* Isim kopyala */
    uint32_t i;
    for (i = 0; i < TASK_NAME_LEN - 1 && name && name[i]; i++)
        t->task_name[i] = name[i];
    t->task_name[i] = '\0';

    /*
     * Yigin hazirla
     * TempleOS: LEA ESP, CCPU.start_stk+sizeof[ESI] mantigi.
     *
     * x86-64 ABI: RSP 16-byte hizali olmali (call oncesi).
     * Giris fonksiyonu dogrudan cagrilacagi icin sahte bir
     * return adresi koyuyoruz (0 = hata yakalar).
     *
     * Yigin duzeni (asagidan yukari):
     *   [stk_top - 8]  : sahte return adresi (0)
     *   rsp = stk_top - 8
     */
    uint64_t stk_top = (uint64_t)(uintptr_t)(stk + stk_pages * 4096);
    stk_top &= ~0xFULL;         /* 16-byte hizala */
    stk_top -= 8;               /* Sahte return adresi icin yer */
      *(uint64_t *)(uintptr_t)stk_top = (uint64_t)(uintptr_t)task_auto_exit;
		    /* Register baslangic degerleri */
    t->rip    = (uint64_t)(uintptr_t)entry;
    t->rsp    = stk_top;
    t->rflags = 0x202;  /* IF=1 (interrupt aktif), rezerv bit=1 */
    t->rbp    = 0;

    return t;
}

/* ------------------------------------------------
 * sched_add()
 * TempleOS: Spawn() sonrasi gorev zincire eklenir.
 * ------------------------------------------------ */
void sched_add(task_t *t) {
    if (!t) return;
    list_add(t);
}

/* ------------------------------------------------
 * sched_remove()
 * TempleOS TaskKillDying() mantigindan uyarlanmistir.
 * ------------------------------------------------ */
void sched_remove(task_t *t) {
    if (!t) return;
    list_remove(t);
}

/* ------------------------------------------------
 * task_kill()
 * TempleOS TASKf_KILL_TASK ile ayni mantik.
 * ------------------------------------------------ */
void task_kill(task_t *t) {
    if (!t) return;
    TASK_FLAG_SET(t, TASKf_KILL_TASK);
}

/* ------------------------------------------------
 * task_sleep()
 * TempleOS wake_jiffy mantigi:
 *   wake_jiffy = total_jiffies + sure
 *   TASKf_SUSPENDED set et
 *   Yield
 * ------------------------------------------------ */
void task_sleep(uint64_t jiffies) {
    if (!current_task) return;
    current_task->wake_jiffy = cpu0.total_jiffies + jiffies;
    TASK_FLAG_SET(current_task, TASKf_SUSPENDED);
    sched_yield();
}

/* ------------------------------------------------
 * task_yield()
 * TempleOS Yield() karsiligi.
 * ------------------------------------------------ */
void task_yield(void) {
    sched_yield();
}

/* ------------------------------------------------
 * sched_next() - Siradaki calisacak gorevi sec
 * TempleOS RESTORE_RSI_TASK / RESTORE_SETH_TASK_IF_READY
 * mantigindan uyarlanmistir.
 *
 * Siralama:
 *   1. Olum bekleyenleri temizle (TASKf_KILL_TASK)
 *   2. Uyumakta olanlari kontrol et (wake_jiffy)
 *   3. Ilk READY goreve gec (TASKf_SUSPENDED degil)
 *   4. Hicbir gorev yoksa idle'a gec
 * ------------------------------------------------ */
static task_t *sched_next(void) {
    if (!task_list_head) return cpu0.idle_task;

    task_t *start = current_task->next_task;
    task_t *t     = start;

    do {
        /* Olum bekleyeni temizle */
        if (TASK_FLAG_TEST(t, TASKf_KILL_TASK) &&
            t != cpu0.idle_task) {
            task_t *victim = t;
            t = t->next_task;
            list_remove(victim);
            /* Yigin ve yapiyi serbest birak */
            uint64_t stk_pages  = victim->stk_size / 4096;
            uint64_t task_pages = (sizeof(task_t) + 4095) / 4096;

            if (victim->image_base) {
                free_pages(victim->image_base, victim->image_pages);
            }

            free_pages(victim->stk_base, stk_pages);
            free_pages(victim, task_pages);
            continue;
        }

        /* Uyuyan gorevi kontrol et */
        if (TASK_FLAG_TEST(t, TASKf_SUSPENDED)) {
            if (cpu0.total_jiffies >= t->wake_jiffy) {
                TASK_FLAG_CLR(t, TASKf_SUSPENDED);
                t->wake_jiffy = 0;
            } else {
                t = t->next_task;
                continue;
            }
        }

        /* READY: bu goreve gec */
        return t;

    } while (t != start);

    /* Hicbir gorev hazir degil -> idle */
    return cpu0.idle_task;
}

/* ------------------------------------------------
 * sched_tick()
 * TempleOS IRQ_TIMER C karsiligi:
 *
 *   LOCK INC CCPU.total_jiffies    -> cpu0.total_jiffies++
 *   BT TASKf_IDLE -> idle_pt_hits  -> bos dongu istatistigi
 *   JMP RESTORE_RSI_TASK           -> context switch
 * ------------------------------------------------ */
void sched_tick(void) {
    /* TempleOS: LOCK INC U64 CCPU.total_jiffies[RDI] */
    cpu0.total_jiffies++;

    /* Bos dongu istatistigi (TempleOS CCPU.idle_pt_hits) */
    if (current_task && TASK_FLAG_TEST(current_task, TASKf_IDLE))
        cpu0.idle_pt_hits++;

    /* Dilim sayacini azalt */
    if (tick_counter > 0) {
        tick_counter--;
        return;
    }

    /* Dilim bitti: sonraki gorevi sec */
    tick_counter = SCHED_TICK_SLICE;

    task_t *next = sched_next();
    if (!next || next == current_task) return;

    /* Context switch */
    task_t *prev  = current_task;
    current_task  = next;
    cpu0.current_task = next;
    cpu0.swap_cnter++;
    next->swap_cnter++;

    sched_context_switch(prev, next);
}

/* ------------------------------------------------
 * sched_yield()
 * Hemen context switch tetikle.
 * ------------------------------------------------ */
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

/* ------------------------------------------------
 * sched_get_current()
 * ------------------------------------------------ */
task_t *sched_get_current(void) {
    return current_task;
}

/* ------------------------------------------------
 * sched_get_task_count()
 * ------------------------------------------------ */
uint32_t sched_get_task_count(void) {
    return task_count;
}

/* ------------------------------------------------
 * sched_print_tasks() - Debug ciktisi
 * TempleOS DrvRep() benzeri
 * ------------------------------------------------ */
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
