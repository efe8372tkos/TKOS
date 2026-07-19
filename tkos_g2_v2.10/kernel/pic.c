#include "pic.h"
#include "types.h"









static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}





static inline void io_wait(void) {
    __asm__ volatile ("outb %%al, $0x80" : : "a"(0));
}
















void pic_init(void) {
    

    
    outb(PIC1_CMD,  PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD,  PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    


    outb(PIC1_DATA, PIC1_OFFSET);   
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);   
    io_wait();

    


    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    


    outb(PIC1_DATA, 0x0D);
    io_wait();
    outb(PIC2_DATA, 0x09);
    io_wait();

    










    outb(PIC1_DATA, 0xFA);
    io_wait();
    outb(PIC2_DATA, 0xFF);
    io_wait();
}








void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);   
    }
    outb(PIC1_CMD, PIC_EOI);       
}




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




uint16_t pic_get_mask(void) {
    return ((uint16_t)inb(PIC2_DATA) << 8) | inb(PIC1_DATA);
}





void pic_disable(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
