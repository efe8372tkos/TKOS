#include "pit.h"
#include "idt.h"
#include "pic.h"
#include "types.h"
#include "sched.h"
#include "marquee.h"
volatile uint64_t pit_ticks = 0;
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
void pit_init(void) {
    uint16_t divisor = (uint16_t)PIT_DIVISOR;
    outb(PIT_CMD, PIT_CMD_CH0_RATE);
    outb(PIT_CH0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH0, (uint8_t)((divisor >> 8) & 0xFF));
    idt_set_handler(I_TIMER, pit_irq_handler);
    pic_unmask_irq(IRQ_TIMER);
}
uint64_t pit_get_ticks(void) {
    return pit_ticks;
}
void pit_sleep_ms(uint64_t ms) {
    uint64_t target = pit_ticks + ms;
    while (pit_ticks < target) {
        __asm__ volatile ("hlt");
    }
}
void pit_irq_handler(int_frame_t *frame) {
    (void)frame;    
    pit_ticks++;
    sched_tick();
    marquee_tick();	    
}
