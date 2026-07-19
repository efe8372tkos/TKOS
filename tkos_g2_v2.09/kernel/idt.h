#pragma once
#include "types.h"

/*
 * TKOS - Interrupt Descriptor Table (IDT)
 * TempleOS kints.HC IntEntrySet/Get, IntInit1/2,
 * KernelA.HH IDTET_* tanimlari esas alinmistir.
 *
 * IDT Vektor Haritasi (TempleOS ile uyumlu):
 *   0x00-0x1F  CPU exception'lari (Intel rezervi)
 *   0x20-0x2F  Donanim IRQ'lari   (PIC yeniden vektorlenmis)
 *   0x30       I_MP_CRASH          (TempleOS uyumlu, gelecek)
 *   0x31       I_WAKE              (TempleOS uyumlu, gelecek)
 *   0x40+      Kullanici tanimli
 */

/* TempleOS KernelA.HH IDTET_* ile ayni degerler */
#define IDTET_TASK  0x05    /* Task gate        */
#define IDTET_IRQ   0x0E    /* Interrupt gate   (CLI yapar) */
#define IDTET_TRAP  0x0F    /* Trap gate        (CLI yapmaz) */

/* TempleOS I_* interrupt numaralari ile uyumlu */
#define I_DIV_ZERO      0x00
#define I_SINGLE_STEP   0x01
#define I_NMI           0x02
#define I_BPT           0x03
#define I_OVERFLOW      0x04
#define I_BOUND         0x05
#define I_BAD_OPCODE    0x06
#define I_NO_MATH       0x07
#define I_DOUBLE_FAULT  0x08
#define I_GPF           0x0D
#define I_PAGE_FAULT    0x0E
#define I_MATH_FAULT    0x10
#define I_TIMER         0x20    /* IRQ0 -> PIC offset 0x20 */
#define I_KEYBOARD      0x21    /* IRQ1 -> PIC offset 0x20 */
#define I_MOUSE         0x2C    /* IRQ12 -> PS/2 Fare */
#define I_MP_CRASH      0x30    /* TempleOS uyumlu */
#define I_WAKE          0x31    /* TempleOS uyumlu */
#define I_USER          0x40    /* Kullanici tanimli baslangic */
#define I_SYSCALL       0x80    /* Sistem cagrisi - int 0x80 */

/* IDT toplam giris sayisi */
#define IDT_ENTRIES     256

/*
 * IDT Gate Descriptor - 64-bit Long Mode (16 byte)
 *
 * [ 0.. 1] offset_low   : handler adresinin bit 0-15
 * [ 2.. 3] selector     : kod segment secici (GDT CS64)
 * [ 4]     ist          : Interrupt Stack Table indeksi (0=yok)
 * [ 5]     type_attr    : tip + DPL + present bit
 * [ 6.. 7] offset_mid   : handler adresinin bit 16-31
 * [ 8..11] offset_high  : handler adresinin bit 32-63
 * [12..15] zero         : rezerv, sifir olmali
 *
 * type_attr bit yapisi:
 *   [0..3] tip    (IDTET_IRQ=0x0E, IDTET_TRAP=0x0F)
 *   [4]    0      (her zaman 0)
 *   [5..6] DPL    (0=kernel, 3=user)
 *   [7]    P      (present=1)
 *
 * DPL=0, present=1, IRQ gate icin: 0x8E
 * DPL=3, present=1, IRQ gate icin: 0xEE
 */
typedef struct __attribute__((packed)) {
    uint16_t offset_low;    /* Handler adresi [15:0]  */
    uint16_t selector;      /* GDT kod segment        */
    uint8_t  ist;           /* IST indeksi (0=devre disi) */
    uint8_t  type_attr;     /* Tip + DPL + Present    */
    uint16_t offset_mid;    /* Handler adresi [31:16] */
    uint32_t offset_high;   /* Handler adresi [63:32] */
    uint32_t zero;          /* Rezerv                 */
} idt_gate_t;

/*
 * IDT Pointer - lidt komutu icin
 * CSysLimitBase (TempleOS) ile ayni yapisal mantik
 */
typedef struct __attribute__((packed)) {
    uint16_t limit;         /* IDT boyutu - 1         */
    uint64_t base;          /* IDT'nin fiziksel adresi */
} idt_ptr_t;

/*
 * CPU'nun interrupt sirasinda stack'e ittigi yapiyi
 * temsil eder. Fault handler'larinda kullanilir.
 * TempleOS CTask.rip/rflags/rsp alanlarindan esinlenilmistir.
 */
typedef struct __attribute__((packed)) {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} int_frame_t;

/* Handler fonksiyon tipi */
typedef void (*int_handler_t)(int_frame_t *frame);

/*
 * idt_init() - IDT'yi kurar.
 * TempleOS IntInit1() + IntInit2() mantigiyla:
 *   1. Tum girisleri int_nop handler'iyla doldurur.
 *   2. CPU exception handler'larini kurar (0x00-0x1F).
 *   3. IRQ handler'larini kurar (0x20-0x2F).
 *   4. lidt ile yukler.
 */
void idt_init(void);

/*
 * idt_set_gate() - Tek bir IDT girisi yazar.
 * TempleOS IntEntrySet() ile ayni mantik:
 *   - handler adresini 3 parcaya boler (low/mid/high)
 *   - selector, type_attr, ist degerlerini yazar
 *
 * @num      : vektor numarasi (0-255)
 * @handler  : handler fonksiyonunun adresi
 * @selector : GDT CS64 segment secici
 * @type     : IDTET_IRQ veya IDTET_TRAP
 * @dpl      : 0 (kernel) veya 3 (user)
 */
void idt_set_gate(uint8_t num, uint64_t handler,
                  uint16_t selector, uint8_t type, uint8_t dpl);

/*
 * idt_set_handler() - Yuksek seviyeli handler kayit fonksiyonu.
 * C handler fonksiyonunu belirtilen interrupt vektorune baglar.
 */
void idt_set_handler(uint8_t num, int_handler_t handler);

/* ------------------------------------------------
 * Assembly stub'lari - isr_stubs.asm'de tanimli
 * Her exception/IRQ icin ayri stub gereklidir cunku
 * bazi exception'lar error code iter, bazilari itmez.
 * TempleOS IntFaultHndlrsNew() runtime kod uretiminin
 * derleme-zamani karsiligi.
 * ------------------------------------------------ */
extern void isr0(void);   /* Division by Zero      */
extern void isr1(void);   /* Single Step / Debug   */
extern void isr2(void);   /* NMI                   */
extern void isr3(void);   /* Breakpoint            */
extern void isr4(void);   /* Overflow              */
extern void isr5(void);   /* Bound Range           */
extern void isr6(void);   /* Invalid Opcode        */
extern void isr7(void);   /* Device Not Available  */
extern void isr8(void);   /* Double Fault          */
extern void isr13(void);  /* General Protection    */
extern void isr14(void);  /* Page Fault            */
extern void isr16(void);  /* x87 FP Exception      */
extern void irq0(void);   /* Timer                 */
extern void irq1(void);   /* Keyboard              */
extern void irq12(void);  /* PS/2 Fare             */
extern void irq_nop(void);/* Bilinmeyen IRQ (NOP)  */
extern void isr_syscall(void); /* int 0x80 - Syscall (bkz. syscall.h) */
