#include "pit.h"
#include "idt.h"
#include "pic.h"
#include "types.h"
#include "sched.h"
#include "marquee.h"

/*
 * TKOS - PIT Implementasyonu
 * TempleOS KMain.HC TimersInit() fonksiyonundan uyarlanmistir.
 *
 * TempleOS total_jiffies (CCPU.total_jiffies) karsiligi
 * olarak pit_ticks kullaniyoruz. TempleOS'ta bu deger
 * IRQ_TIMER icinde LOCK INC ile atomik arttirilir;
 * biz simdilik tek cekirdek oldugumuz icin basit
 * increment kullaniyoruz.
 */

/* ------------------------------------------------
 * Sistem tik sayaci
 * TempleOS: CCPU.total_jiffies karsiligi
 * volatile: derleyici optimize etmesin
 * ------------------------------------------------ */
volatile uint64_t pit_ticks = 0;

/* ------------------------------------------------
 * Port I/O yardimcilari
 * ------------------------------------------------ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ------------------------------------------------
 * pit_init()
 * TempleOS TimersInit() ilk bolumu ile birebir ayni:
 *
 *   OutU8(0x43, 0x34)                     ; Mod komutu
 *   OutU8(0x40, SYS_TIMER0_PERIOD & 0xFF) ; Dusuk byte
 *   OutU8(0x40, SYS_TIMER0_PERIOD >> 8)   ; Yuksek byte
 *
 * TempleOS HPET kurulumu da var ancak o PCI erisimine
 * gerek duyar; biz simdilik sadece PIT kullaniyoruz.
 * HPET ileride eklenebilir (pit.c genisletilerek).
 * ------------------------------------------------ */
void pit_init(void) {
    uint16_t divisor = (uint16_t)PIT_DIVISOR;

    /* Mod komutu: Kanal 0, low+high, Mod 2 (rate generator), binary */
    outb(PIT_CMD, PIT_CMD_CH0_RATE);

    /* Bolme degerinin dusuk ve yuksek byte'larini gonder */
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));

    /* IRQ0 handler'ini IDT'ye bagla */
    idt_set_handler(I_TIMER, pit_irq_handler);

    /* IRQ0'i unmask et */
    pic_unmask_irq(IRQ_TIMER);
}

/* ------------------------------------------------
 * pit_get_ticks()
 * TempleOS cnts.HPET veya total_jiffies okuma
 * mantigindan esinlenilmistir.
 * ------------------------------------------------ */
uint64_t pit_get_ticks(void) {
    return pit_ticks;
}

/* ------------------------------------------------
 * pit_sleep_ms()
 * TempleOS Busy() fonksiyonunun basit karsiligi.
 *
 * TempleOS Busy(us) mikrosaniye bekler; biz
 * milisaniye kullaniyoruz (PIT_HZ=1000 ile 1:1).
 *
 * hlt ile bekliyoruz: interrupt geldiginde devam
 * eder, boylece CPU bosuna donmez.
 * ------------------------------------------------ */
void pit_sleep_ms(uint64_t ms) {
    uint64_t target = pit_ticks + ms;
    while (pit_ticks < target) {
        __asm__ volatile ("hlt");
    }
}

/* ------------------------------------------------
 * pit_irq_handler()
 * TempleOS IRQ_TIMER mantigindan uyarlanmistir:
 *
 *   LOCK INC U64 CCPU.total_jiffies[RDI]
 *
 * Simdilik sadece sayaci artir.
 * Scheduler entegrasyonu task.c/sched.c tamamlandiginda
 * buraya eklenecek: sched_tick() cagrisi.
 * EOI pic.c idt_dispatch() tarafindan gonderilir.
 * ------------------------------------------------ */
void pit_irq_handler(int_frame_t *frame) {
    (void)frame;    /* Henuz kullanilmiyor */
    pit_ticks++;
    sched_tick();
    marquee_tick();	    
}
