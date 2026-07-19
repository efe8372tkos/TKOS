#pragma once
#include "types.h"

/*
 * TKOS - Task ve CPU Yapilari
 * TempleOS KernelA.HH CTask ve CCPU struct'larindan
 * dogrudan uyarlanmistir. Gereksiz alanlar (pencere,
 * derleyici, debug, ODE, multicore) cikarilmis;
 * temel zamanlayici ve context switch alanlari korunmustur.
 */

/* ------------------------------------------------
 * Sabitler
 * TempleOS KernelA.HH ile uyumlu
 * ------------------------------------------------ */
#define TASK_NAME_LEN       32
#define TASK_SIGNATURE_VAL  0x546B5353UL    /* 'TkSS' */

#define TASK_STK_SIZE       (4096 * 8)      /* 32KB - varsayilan yigin */
#define TASK_MAX            64              /* Maksimum es zamanli gorev */

/* ------------------------------------------------
 * TASKf_* - CTask.task_flags bit konumlari
 * TempleOS TASKf_* ile birebir ayni degerler
 * ------------------------------------------------ */
#define TASKf_TASK_LOCK     0   /* Gorev kilidi        */
#define TASKf_KILL_TASK     1   /* Oldurmek icin isaret */
#define TASKf_SUSPENDED     2   /* Askida              */
#define TASKf_IDLE          3   /* Bos dongu gorevi    */
#define TASKf_AWAITING_MSG  9   /* Mesaj bekliyor      */

/* Bayrak makrolari */
#define TASK_FLAG_SET(t, f)   ((t)->task_flags |=  (1U << (f)))
#define TASK_FLAG_CLR(t, f)   ((t)->task_flags &= ~(1U << (f)))
#define TASK_FLAG_TEST(t, f)  (((t)->task_flags >> (f)) & 1U)

/* ------------------------------------------------
 * FPU/SSE kayit alani (512 byte - fxsave/fxrstor)
 * TempleOS CFPU ile ayni boyut
 * ------------------------------------------------ */
typedef struct {
    uint8_t body[512];
} __attribute__((aligned(16))) fpu_state_t;

/* ------------------------------------------------
 * CTask - Gorev kontrol blogu
 *
 * TempleOS CTask'tan uyarlandi.
 * En kritik kisim: register kayit alani.
 * Bu alanlar TASK_CONTEXT_SAVE / _TASK_CONTEXT_RESTORE
 * assembly kodunun beklettigi sirayla ayni olmali.
 *
 * TempleOS CTask register sirasi (3302. satir):
 *   rip, rflags, rsp, rsi, rax, rcx, rdx, rbx, rbp, rdi,
 *   r8, r9, r10, r11, r12, r13, r14, r15
 * ------------------------------------------------ */
typedef struct task {
    /* Kimlik */
    struct task *addr;              /* Self-pointer (TempleOS CTask.addr) */
    uint32_t    task_signature;     /* TASK_SIGNATURE_VAL                 */
    uint32_t    task_flags;         /* TASKf_* bayraklari                 */

    /* Gorev bilgisi */
    char        task_name[TASK_NAME_LEN];
    uint32_t    task_num;           /* Benzersiz gorev numarasi           */

    /*
     * CPU Register kayit alani
     * TempleOS CTask register sirasi ile BIREBIR AYNI.
     * sched_context_switch() bu alanlari kullanir.
     *
     *   rip    : Gorev devam edecegi adres
     *   rflags : RFLAGS register degeri
     *   rsp    : Yigin isareti
     *   rsi..r15: Genel amacli register'lar
     */
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

    /* FPU/SSE durumu */
    fpu_state_t *fpu_state;         /* fxsave/fxrstor icin alan          */

    /* Zamanlayici */
    uint64_t    wake_jiffy;         /* Bu jiffy'de uyan (TempleOS ayni)  */
    uint64_t    total_jiffies;      /* Calistigi toplam jiffy sayisi      */
    uint64_t    swap_cnter;         /* Context switch sayaci              */

    /* Baglantili liste (TempleOS next_task/last_task) */
    struct task *next_task;
    struct task *last_task;

    /* Yigin */
    uint8_t     *stk_base;          /* Yigin alani baslangici             */
    uint64_t    stk_size;           /* Yigin boyutu (byte)                */

    void        *image_base;
    uint64_t    image_pages;

    /* Kullanici verisi */
    uint64_t    user_data;
} task_t;

/* ------------------------------------------------
 * CCPU - CPU kontrol blogu
 *
 * TempleOS CCPU'dan uyarlandi.
 * Simdilik tek cekirdek; ileride SMP icin genisletilebilir.
 * ------------------------------------------------ */
typedef struct {
    /* Self-pointer (TempleOS CCPU.addr) */
    void       *addr;

    /* Cekirdek numarasi (TempleOS CCPU.num) */
    uint64_t    num;

    /* Sayaclar (TempleOS CCPU.total_jiffies, idle_pt_hits) */
    uint64_t    total_jiffies;
    uint64_t    idle_pt_hits;

    /* Gorev isaretcileri (TempleOS CCPU.seth_task, idle_task) */
    task_t     *current_task;       /* Simdi calisan gorev               */
    task_t     *idle_task;          /* Bos dongu gorevi                  */

    /* Zamanlayici sayaci */
    uint64_t    swap_cnter;
} cpu_t;

/* ------------------------------------------------
 * Global CPU yapisi (tek cekirdek)
 * TempleOS: GS segment register -> CCPU
 * Biz: dogrudan global pointer
 * ------------------------------------------------ */
extern cpu_t cpu0;
extern task_t *current_task;

/* ------------------------------------------------
 * task_create() - Yeni gorev olustur.
 * TempleOS Spawn() mantigindan uyarlanmistir.
 *
 * @name     : Gorev adi (TASK_NAME_LEN-1 karakter maks)
 * @entry    : Gorev giris fonksiyonu
 * @stk_size : Yigin boyutu (0 = TASK_STK_SIZE varsayilan)
 *
 * Basarisizsa NULL dondurur.
 */
task_t *task_create(const char *name,
                    void (*entry)(void),
                    uint64_t stk_size);

/*
 * task_kill() - Gorevi oldurmek icin isaretle.
 * TempleOS TASKf_KILL_TASK ile ayni mantik.
 */
void task_kill(task_t *t);

/*
 * task_sleep() - Gorevi N jiffy sure uyut.
 * TempleOS wake_jiffy mantigi ile ayni.
 * @jiffies: uyku suresi (1 jiffy = 1ms, PIT_HZ=1000 ile)
 */
void task_sleep(uint64_t jiffies);

/*
 * task_yield() - CPU'yu gonullu birak.
 * TempleOS Yield() karsiligi. Zamanlayiciya gecis.
 */
void task_yield(void);
