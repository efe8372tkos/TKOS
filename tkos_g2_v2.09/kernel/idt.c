#include "idt.h"
#include "pic.h"
#include "fb_console.h"
#include "types.h"

/*
 * TKOS - IDT Implementasyonu
 * TempleOS kints.HC IntInit1/2, IntEntrySet/Get,
 * IntNop, IntFaultHndlrsNew mantigindan uyarlanmistir.
 */

/* ------------------------------------------------
 * IDT tablosu ve pointer'i
 * TempleOS: dev.idt = CAlloc(16*256)
 * Biz: statik dizi (heap yok henuz)
 * ------------------------------------------------ */
static idt_gate_t idt[IDT_ENTRIES];
static idt_ptr_t  idt_ptr;

/*
 * C seviyesi handler tablosu.
 * TempleOS'un IntEntrySet() ile kayitli fonksiyon
 * pointer'larina karsilik gelir.
 */
static int_handler_t c_handlers[IDT_ENTRIES];

/* GDT'deki 64-bit kod segment secici (stage2.asm ile uyumlu) */
#define CODE64_SEG 0x18

/* ------------------------------------------------
 * lidt - IDT'yi yukle
 * TempleOS: LIDT U64 [RAX]
 * ------------------------------------------------ */
static inline void lidt(idt_ptr_t *ptr) {
    __asm__ volatile ("lidt (%0)" : : "r"(ptr));
}

static inline void sti(void) {
    __asm__ volatile ("sti");
}

static inline void cli(void) {
    __asm__ volatile ("cli");
}

/* ------------------------------------------------
 * idt_set_gate()
 * TempleOS IntEntrySet() ile ayni mantik:
 *   fp.u16[0] -> offset_low
 *   fp.u16[1] -> offset_mid
 *   fp.u32[1] -> offset_high
 *   cs64 secici -> selector
 *   0x8000 + type<<8 + dpl<<13 -> type_attr
 * ------------------------------------------------ */
void idt_set_gate(uint8_t num, uint64_t handler,
                  uint16_t selector, uint8_t type, uint8_t dpl) {
    idt[num].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[num].selector    = selector;
    idt[num].ist         = 0;
    idt[num].type_attr   = (uint8_t)(0x80 | ((dpl & 0x3) << 5) | (type & 0xF));
    idt[num].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[num].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[num].zero        = 0;
}

void idt_set_handler(uint8_t num, int_handler_t handler) {
    c_handlers[num] = handler;
}

/* ------------------------------------------------
 * Varsayilan (NOP) handler
 * TempleOS IntNop() ile ayni mantik:
 *   OutU8(0xA0, 0x20)  slave EOI
 *   OutU8(0x20, 0x20)  master EOI
 * IRQ numarasini bulmak icin vektor - 0x20 kullaniriz.
 * ------------------------------------------------ */
static void default_irq_handler(int_frame_t *frame) {
    (void)frame;
    /* Bilinmeyen IRQ'ya sessizce EOI gonder */
    pic_send_eoi(7); /* Sahte IRQ7 varsayimi - TempleOS IntNop gibi */
}

/* ------------------------------------------------
 * Exception mesaj tablosu
 * TempleOS ST_INT_NAMES'ten esinlenilmistir.
 * ------------------------------------------------ */
static const char *exception_names[] = {
    "Division by Zero",         /* 0x00 I_DIV_ZERO    */
    "Single Step",              /* 0x01 I_SINGLE_STEP */
    "NMI",                      /* 0x02 I_NMI         */
    "Breakpoint",               /* 0x03 I_BPT         */
    "Overflow",                 /* 0x04               */
    "Bound Range Exceeded",     /* 0x05               */
    "Invalid Opcode",           /* 0x06               */
    "Device Not Available",     /* 0x07               */
    "Double Fault",             /* 0x08               */
    "Reserved",                 /* 0x09               */
    "Invalid TSS",              /* 0x0A               */
    "Segment Not Present",      /* 0x0B               */
    "Stack Fault",              /* 0x0C               */
    "General Protection Fault", /* 0x0D               */
    "Page Fault",               /* 0x0E I_PAGE_FAULT  */
    "Reserved",                 /* 0x0F               */
    "x87 FP Exception",         /* 0x10               */
    "Alignment Check",          /* 0x11               */
    "Machine Check",            /* 0x12               */
    "SIMD FP Exception",        /* 0x13               */
};
#define EXCEPTION_NAMES_COUNT 20

/*
 * Ortak exception handler (C seviyesi)
 * TempleOS INT_FAULT + Fault2() mantigindan uyarlanmistir:
 *   fault_num -> exception turu
 *   rip       -> hata adresi
 * Simdilik: ekrana yazdir ve sistemi durdur.
 */
static void exception_handler(int_frame_t *frame, uint8_t num) {
    cli();
    con_set_color(12, 0); /* Kirmizi yazi, siyah arka plan */
    con_print("\n\n*** TKOS KERNEL PANIC ***\n");
    con_print("Exception: ");
    if (num < EXCEPTION_NAMES_COUNT)
        con_print(exception_names[num]);
    else
        con_print("Unknown");
    con_print(" (");
    con_print_hex(num);
    con_print(")\n");
    con_print("RIP: ");
    con_print_hex(frame->rip);
    con_print("\nRFLAGS: ");
    con_print_hex(frame->rflags);
    con_print("\nRSP: ");
    con_print_hex(frame->rsp);
    con_print("\n\nSystem Halted.\n");
    for (;;) __asm__ volatile ("hlt");
}

/* ------------------------------------------------
 * Ortak dispatch fonksiyonu
 * Assembly stub'larindan cagrilir.
 * TempleOS INT_FAULT dispatch mantigindan uyarlanmistir.
 * ------------------------------------------------ */
void idt_dispatch(uint64_t num, int_frame_t *frame) {
    if (num < 0x20) {
        /* CPU exception */
        if (c_handlers[num]) {
            c_handlers[num](frame);
        } else {
            exception_handler(frame, (uint8_t)num);
        }
    } else {
        /* Donanim IRQ veya yazilim interrupt */
        if (c_handlers[num]) {
            c_handlers[num](frame);
        } else {
            default_irq_handler(frame);
        }
        /* IRQ ise EOI gonder */
        if (num >= 0x20 && num <= 0x2F) {
            pic_send_eoi((uint8_t)(num - 0x20));
        }
    }
}

/* ------------------------------------------------
 * idt_init()
 * TempleOS IntInit1() + IntInit2() mantigiyla:
 *
 * IntInit1: Tum girisleri IntNop ile doldur, lidt yukle.
 * IntInit2: Exception ve IRQ handler'larini kur.
 * ------------------------------------------------ */
void idt_init(void) {
    uint32_t i;

    /* c_handlers tablosunu sifirla */
    for (i = 0; i < IDT_ENTRIES; i++)
        c_handlers[i] = 0;

    /* -----------------------------------------------
     * IntInit1 karsiligi:
     * Tum 256 girisi irq_nop stub'iyla doldur.
     * TempleOS: for(i=0;i<256;i++) IntEntrySet(i,&IntNop)
     * ----------------------------------------------- */
    for (i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((uint8_t)i,
                     (uint64_t)(uintptr_t)irq_nop,
                     CODE64_SEG,
                     IDTET_IRQ,
                     0);
    }

    /* -----------------------------------------------
     * IntInit2 karsiligi:
     * CPU exception'lari (0x00-0x1F)
     * TempleOS: IntEntrySet(I_DIV_ZERO, &IntDivZero) vb.
     * ----------------------------------------------- */
    idt_set_gate(0x00, (uint64_t)(uintptr_t)isr0,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x01, (uint64_t)(uintptr_t)isr1,   CODE64_SEG, IDTET_TRAP, 0);
    idt_set_gate(0x02, (uint64_t)(uintptr_t)isr2,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x03, (uint64_t)(uintptr_t)isr3,   CODE64_SEG, IDTET_TRAP, 3); /* BPT: DPL=3 */
    idt_set_gate(0x04, (uint64_t)(uintptr_t)isr4,   CODE64_SEG, IDTET_TRAP, 0);
    idt_set_gate(0x05, (uint64_t)(uintptr_t)isr5,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x06, (uint64_t)(uintptr_t)isr6,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x07, (uint64_t)(uintptr_t)isr7,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x08, (uint64_t)(uintptr_t)isr8,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x0D, (uint64_t)(uintptr_t)isr13,  CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x0E, (uint64_t)(uintptr_t)isr14,  CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x10, (uint64_t)(uintptr_t)isr16,  CODE64_SEG, IDTET_IRQ,  0);

    /* -----------------------------------------------
     * Donanim IRQ'lari (0x20-0x2F)
     * TempleOS: IntEntrySet(I_TIMER, IRQ_TIMER) vb.
     * ----------------------------------------------- */
    idt_set_gate(I_TIMER,    (uint64_t)(uintptr_t)irq0,  CODE64_SEG, IDTET_IRQ, 0);
    idt_set_gate(I_KEYBOARD, (uint64_t)(uintptr_t)irq1,  CODE64_SEG, IDTET_IRQ, 0);
    idt_set_gate(I_MOUSE,    (uint64_t)(uintptr_t)irq12, CODE64_SEG, IDTET_IRQ, 0);

    /* -----------------------------------------------
     * IDT pointer'i hazirla ve yukle
     * TempleOS IntInit1: tmp_ptr.limit = 256*16-1
     *                    tmp_ptr.base  = dev.idt
     *                    LIDT [RAX]
     * ----------------------------------------------- */
    idt_ptr.limit = (uint16_t)(sizeof(idt_gate_t) * IDT_ENTRIES - 1);
    idt_ptr.base  = (uint64_t)(uintptr_t)idt;
    idt_set_gate(I_SYSCALL, (uint64_t)(uintptr_t)isr_syscall,
      CODE64_SEG, IDTET_TRAP, 0);
    lidt(&idt_ptr);
}
