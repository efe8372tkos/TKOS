#pragma once
#include "types.h"
#include "idt.h"

/*
 * TKOS - Programmable Interval Timer (Intel 8253/8254)
 * TempleOS KMain.HC TimersInit() fonksiyonundan uyarlanmistir.
 *
 * PIT Kanallari:
 *   Kanal 0 (0x40) -> IRQ0, sistem zamanlayicisi
 *   Kanal 1 (0x41) -> Eski RAM refresh, artik kullanilmiyor
 *   Kanal 2 (0x42) -> PC hoparlor
 *
 * Temel frekans: 1.193182 MHz (PIT_BASE_FREQ)
 * Bolme degeri  : PIT_BASE_FREQ / istenen_Hz
 *
 * TempleOS SYS_TIMER0_PERIOD ile ayni mantik:
 *   OutU8(0x43, 0x34)                     ; Mod 2, kanal 0
 *   OutU8(0x40, SYS_TIMER0_PERIOD & 0xFF) ; low byte
 *   OutU8(0x40, SYS_TIMER0_PERIOD >> 8)   ; high byte
 */

/* PIT I/O port adresleri */
#define PIT_CH0     0x40    /* Kanal 0 veri portu (IRQ0)  */
#define PIT_CH1     0x41    /* Kanal 1 veri portu         */
#define PIT_CH2     0x42    /* Kanal 2 veri portu (ses)   */
#define PIT_CMD     0x43    /* Mod/komut register'i       */

/* Temel osilatör frekansi (Hz) */
#define PIT_BASE_FREQ   1193182UL

/* Hedef sistem tik frekansi */
#define PIT_HZ          1000        /* 1000 Hz = 1ms cozunurluk */

/*
 * Bolme degeri hesabi:
 *   PIT_BASE_FREQ / PIT_HZ = 1193182 / 1000 = 1193
 * TempleOS SYS_TIMER0_PERIOD karsiligi.
 */
#define PIT_DIVISOR     (PIT_BASE_FREQ / PIT_HZ)

/*
 * Komut byte bit yapisi (PIT_CMD portu):
 *   [1:0] BCD/Binary, mod secimi
 *   [3:1] Calisma modu (Mode 0-5)
 *   [5:4] Erisim modu  (00=latch, 01=low, 10=high, 11=low+high)
 *   [7:6] Kanal secimi (00=ch0, 01=ch1, 10=ch2, 11=readback)
 *
 * 0x34 = 0011 0100b:
 *   Kanal 0, low+high erisim, Mod 2 (rate generator), binary
 * TempleOS: OutU8(0x43, 0x34) ile ayni.
 */
#define PIT_CMD_CH0_RATE    0x34    /* Kanal 0, Mod 2, binary */
#define PIT_CMD_CH2_SQUARE  0xB6    /* Kanal 2, Mod 3 (ses icin) */

/* Sistem tik sayaci - IRQ0 handler tarafindan arttirilir */
extern volatile uint64_t pit_ticks;

/*
 * pit_init() - PIT'i baslatir.
 * TempleOS TimersInit() ilk uclu ile ayni:
 *   OutU8(0x43, 0x34)
 *   OutU8(0x40, SYS_TIMER0_PERIOD & 0xFF)
 *   OutU8(0x40, SYS_TIMER0_PERIOD >> 8)
 *
 * PIT_HZ frekansta IRQ0 uretir.
 */
void pit_init(void);

/*
 * pit_get_ticks() - Gecen tik sayisini dondurur.
 * Her IRQ0'da pit_ticks 1 artar.
 * 1000 Hz ayarinda: 1 tik = 1 ms
 */
uint64_t pit_get_ticks(void);

/*
 * pit_sleep_ms() - Milisaniye cinsinden bekler.
 * TempleOS Busy() fonksiyonunun basit karsiligi.
 * Interrupt'lar acik olmalidir (STI sonrasi).
 *
 * @ms : bekleme suresi (milisaniye)
 */
void pit_sleep_ms(uint64_t ms);

/*
 * pit_irq_handler() - IRQ0 C handler'i.
 * idt_set_handler(I_TIMER, pit_irq_handler) ile
 * IDT'ye baglanir.
 * pit_ticks sayacini artirir.
 */
void pit_irq_handler(int_frame_t *frame);
