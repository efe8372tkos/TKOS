#pragma once
#include "types.h"

/*
 * TKOS - 8259 Programmable Interrupt Controller (PIC)
 * TempleOS kints.HC IntsInit() fonksiyonundan uyarlanmistir.
 *
 * Master PIC : IRQ0-IRQ7  -> INT 0x20-0x27
 * Slave  PIC : IRQ8-IRQ15 -> INT 0x28-0x2F
 *
 * IRQ Haritasi:
 *   IRQ0  (0x20) = PIT Timer
 *   IRQ1  (0x21) = PS/2 Klavye
 *   IRQ2  (0x22) = Slave PIC Cascade (dahili)
 *   IRQ8  (0x28) = RTC
 *   IRQ12 (0x2C) = PS/2 Mouse
 *   IRQ14 (0x2E) = ATA Primary
 *   IRQ15 (0x2F) = ATA Secondary
 */

/* PIC I/O port adresleri */
#define PIC1_CMD    0x20    /* Master PIC komut portu  */
#define PIC1_DATA   0x21    /* Master PIC veri portu   */
#define PIC2_CMD    0xA0    /* Slave  PIC komut portu  */
#define PIC2_DATA   0xA1    /* Slave  PIC veri portu   */

/* PIC EOI (End of Interrupt) komutu */
#define PIC_EOI     0x20

/* ICW (Initialization Command Words) */
#define PIC_ICW1_ICW4   0x01    /* ICW4 gerekli         */
#define PIC_ICW1_INIT   0x10    /* Baslangic komutu     */
#define PIC_ICW4_8086   0x01    /* 8086/88 modu         */
#define PIC_ICW4_AEOI   0x02    /* Auto EOI             */
#define PIC_ICW4_SFNM   0x10    /* Special Fully Nested */

/* Vektor ofseti */
#define PIC1_OFFSET 0x20        /* Master: INT 0x20-0x27 */
#define PIC2_OFFSET 0x28        /* Slave:  INT 0x28-0x2F */

/* IRQ numaralari (0 tabanli) */
#define IRQ_TIMER    0
#define IRQ_KEYBOARD 1
#define IRQ_CASCADE  2
#define IRQ_RTC      8
#define IRQ_MOUSE    12
#define IRQ_ATA1     14
#define IRQ_ATA2     15

/*
 * pic_init() - 8259 PIC'i baslatir ve yeniden vektorler.
 * TempleOS IntsInit() ile ayni mantik:
 *   - ICW1: cascade, edge trigger, ICW4 gerekli
 *   - ICW2: vektor ofseti (master=0x20, slave=0x28)
 *   - ICW3: cascade konfigurasyonu
 *   - ICW4: 8086 modu
 * Varsayilan maske: sadece IRQ0 (timer) ve IRQ1 (klavye) acik.
 */
void pic_init(void);

/*
 * pic_send_eoi() - Interrupt isleyicinin sonunda cagrilir.
 * IRQ >= 8 ise hem slave hem master'a EOI gonderilir.
 */
void pic_send_eoi(uint8_t irq);

/*
 * pic_mask_irq()   - Belirli bir IRQ hattini maskeler (devre disi).
 * pic_unmask_irq() - Belirli bir IRQ hattini acik birakar.
 */
void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);

/*
 * pic_get_mask() - Mevcut IRQ maske degerini dondurur.
 * Bit 0-7: Master, Bit 8-15: Slave
 */
uint16_t pic_get_mask(void);

/*
 * pic_disable() - Her iki PIC'i tamamen maskeler.
 * APIC kullanilacaksa cagrilir.
 */
void pic_disable(void);
