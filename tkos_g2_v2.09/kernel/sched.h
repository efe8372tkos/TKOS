#pragma once
#include "task.h"
#include "types.h"

/*
 * TKOS - Round-Robin Zamanlayici
 * TempleOS Sched.HC / MultiProc.HC mantigindan uyarlanmistir.
 *
 * Yapi:
 *   - Tek yonlu cift bagli dairesel liste (TempleOS CTask.next/last_task)
 *   - Her gorev 1 jiffy (1ms) calisir, sonra siraya geri girer
 *   - IRQ0 (timer) her jiffy'de sched_tick() cagirir
 *   - TASKf_SUSPENDED, TASKf_KILL_TASK, wake_jiffy destegi
 *
 * Context switch tamamen assembly ile yapilir (sched_asm.asm).
 * TempleOS TASK_CONTEXT_SAVE / _TASK_CONTEXT_RESTORE karsiligi.
 */

/* Zamanlayici ayarlari */
#define SCHED_TICK_SLICE    1       /* Her gorev kac jiffy calisir */

/*
 * sched_init() - Zamanlayiciyi baslat.
 * Bos dongu gorevini olusturur, gorev listesini hazirlar.
 * TempleOS Core0Init -> CPUStructInit -> idle_task mantigi.
 */
void sched_init(void);

/*
 * sched_add() - Gorevi zamanlayiciya ekle.
 * TempleOS: Spawn() sonrasi gorev listesine ekleme.
 * Gorev derhal READY durumuna gecer.
 */
void sched_add(task_t *t);

/*
 * sched_remove() - Gorevi zamanlayicidan cikar.
 * TempleOS TaskKillDying() mantigindan uyarlanmistir.
 */
void sched_remove(task_t *t);

/*
 * sched_tick() - Her IRQ0'da cagrilir.
 * TempleOS IRQ_TIMER'in C karsiligi:
 *   - total_jiffies artirir (CCPU.total_jiffies)
 *   - wake_jiffy kontrolu yapar (uyuyan gorevleri uyandirır)
 *   - Dilimleme sayacini azaltir
 *   - Gerekirse context switch tetikler
 */
void sched_tick(void);

/*
 * sched_yield() - Mevcut gorevi gonullu birak.
 * TempleOS Yield() / STI + do{PAUSE}while() karsiligi.
 * Hemen bir sonraki goreve gecer.
 */
void sched_yield(void);

/*
 * sched_get_current() - Simdi calisan gorevi dondur.
 * TempleOS: Fs segment -> CTask*
 */
task_t *sched_get_current(void);

/*
 * sched_get_task_count() - Aktif gorev sayisini dondur.
 */
uint32_t sched_get_task_count(void);

/*
 * sched_print_tasks() - Gorev listesini konsola yaz (debug).
 */
void sched_print_tasks(void);

/* ------------------------------------------------
 * Context switch - assembly'de tanimli (sched_asm.asm)
 * TempleOS TASK_CONTEXT_SAVE / _TASK_CONTEXT_RESTORE
 * karsiligi.
 *
 * sched_context_switch(prev, next):
 *   - prev->rip/rsp/rflags/register'lari kaydet
 *   - next->rip/rsp/rflags/register'lari yukle
 *   - fxsave/fxrstor ile FPU durumu degistir
 * ------------------------------------------------ */
extern void sched_context_switch(task_t *prev, task_t *next);
