#include "pic.h"
#include "types.h"

/*
 * TKOS - 8259 PIC Implementasyonu
 * TempleOS kints.HC IntsInit() fonksiyonundan uyarlanmistir.
 */

/* ------------------------------------------------
 * Port I/O yardimci fonksiyonlari
 * ------------------------------------------------ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*
 * io_wait() - Eski donanim icin kisa gecikme.
 * Port 0x80'e yazma ~1-4us gecikme saglar.
 */
static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}

/* ------------------------------------------------
 * pic_init()
 * TempleOS IntsInit() ile birebir ayni ICW sirasi:
 *
 *   OutU8(0x20, 0x11)  ICW1
 *   OutU8(0xA0, 0x11)  ICW1
 *   OutU8(0x21, 0x20)  ICW2 master offset
 *   OutU8(0xA1, 0x28)  ICW2 slave  offset
 *   OutU8(0x21, 0x04)  ICW3 master: slave on IRQ2
 *   OutU8(0xA1, 0x02)  ICW3 slave:  cascade ID=2
 *   OutU8(0x21, 0x0D)  ICW4 master: 8086 + SFNM + AEOI
 *   OutU8(0xA1, 0x09)  ICW4 slave:  8086 + SFNM
 *   OutU8(0x21, 0xFA)  Maske: sadece IRQ0 ve IRQ2 acik
 *   OutU8(0xA1, 0xFF)  Slave tamamen maskeli
 * ------------------------------------------------ */
void pic_init(void) {
    /* Mevcut maskeleri sakla (restore gerekmez, yeniden kuruyor) */

    /* ICW1: Baslangic komutu - cascade, edge, ICW4 gerekli */
    outb(PIC1_CMD,  PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD,  PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    /* ICW2: Vektor ofseti
     *   Master -> 0x20 (INT 32..39)
     *   Slave  -> 0x28 (INT 40..47)  */
    outb(PIC1_DATA, PIC1_OFFSET);   /* 0x20 */
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);   /* 0x28 */
    io_wait();

    /* ICW3: Cascade konfigurasyonu
     *   Master: IRQ2 hattinda slave var (bit 2 = 0x04)
     *   Slave:  cascade kimlik numarasi = 2           */
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    /* ICW4: 8086 modu
     *   Master: 0x0D = 8086(0x01) + AEOI(0x02) + SFNM(0x10) -- TempleOS ile ayni
     *   Slave:  0x09 = 8086(0x01) + SFNM(0x10)               -- TempleOS ile ayni */
    outb(PIC1_DATA, 0x0D);
    io_wait();
    outb(PIC2_DATA, 0x09);
    io_wait();

    /*
     * IRQ Maskesi (TempleOS ile ayni):
     *   Master 0xFA = 1111 1010b
     *     -> IRQ0 (timer) ACIK, IRQ1 (klavye) KAPALI,
     *        IRQ2 (cascade) ACIK, diger hepsi KAPALI
     *   Slave  0xFF = hepsi KAPALI
     *
     * Not: TempleOS 0xFA kullanir; klavyeyi IDT kurulumu
     * tamamlandiktan sonra pic_unmask_irq(IRQ_KEYBOARD)
     * ile aciyoruz.
     */
    outb(PIC1_DATA, 0xFA);
    io_wait();
    outb(PIC2_DATA, 0xFF);
    io_wait();
}

/* ------------------------------------------------
 * pic_send_eoi()
 * TempleOS IntNop() ve IRQ_TIMER EOI mantigindan
 * uyarlanmistir:
 *   OutU8(0xA0, 0x20)  slave EOI
 *   OutU8(0x20, 0x20)  master EOI
 * ------------------------------------------------ */
void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);   /* Slave'e EOI */
    }
    outb(PIC1_CMD, PIC_EOI);       /* Master'a EOI (her zaman) */
}

/* ------------------------------------------------
 * pic_mask_irq() / pic_unmask_irq()
 * ------------------------------------------------ */
void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  val;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) | (1 << irq);
    outb(port, val);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t  val;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    val = inb(port) & ~(1 << irq);
    outb(port, val);
}

/* ------------------------------------------------
 * pic_get_mask()
 * ------------------------------------------------ */
uint16_t pic_get_mask(void) {
    return ((uint16_t)inb(PIC2_DATA) << 8) | inb(PIC1_DATA);
}

/* ------------------------------------------------
 * pic_disable()
 * Tum IRQ hatlarini maskele (APIC'e gecis icin)
 * ------------------------------------------------ */
void pic_disable(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
