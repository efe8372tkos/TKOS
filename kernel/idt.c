#include "idt.h"
#include "pic.h"
#include "fb_console.h"
#include "types.h"
static idt_gate_t idt[IDT_ENTRIES];
static idt_ptr_t  idt_ptr;
static int_handler_t c_handlers[IDT_ENTRIES];
#define CODE64_SEG 0x18
static inline void lidt(idt_ptr_t *ptr) {
    __asm__ volatile ("lidt (%0)" : : "r"(ptr));
}
static inline void sti(void) {
    __asm__ volatile ("sti");
}
static inline void cli(void) {
    __asm__ volatile ("cli");
}
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
static void default_irq_handler(int_frame_t *frame) {
    (void)frame;
    pic_send_eoi(7); 
}
static const char *exception_names[] = {
    "Division by Zero",         
    "Single Step",              
    "NMI",                      
    "Breakpoint",               
    "Overflow",                 
    "Bound Range Exceeded",     
    "Invalid Opcode",           
    "Device Not Available",     
    "Double Fault",             
    "Reserved",                 
    "Invalid TSS",              
    "Segment Not Present",      
    "Stack Fault",              
    "General Protection Fault", 
    "Page Fault",               
    "Reserved",                 
    "x87 FP Exception",         
    "Alignment Check",          
    "Machine Check",            
    "SIMD FP Exception",        
};
#define EXCEPTION_NAMES_COUNT 20
static void exception_handler(int_frame_t *frame, uint8_t num) {
    cli();
    con_set_color(12, 0); 
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
void idt_dispatch(uint64_t num, int_frame_t *frame) {
    if (num < 0x20) {
        if (c_handlers[num]) {
            c_handlers[num](frame);
        } else {
            exception_handler(frame, (uint8_t)num);
        }
    } else {
        if (c_handlers[num]) {
            c_handlers[num](frame);
        } else {
            default_irq_handler(frame);
        }
        if (num >= 0x20 && num <= 0x2F) {
            pic_send_eoi((uint8_t)(num - 0x20));
        }
    }
}
void idt_init(void) {
    uint32_t i;
    for (i = 0; i < IDT_ENTRIES; i++)
        c_handlers[i] = 0;
    for (i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate((uint8_t)i,
                     (uint64_t)(uintptr_t)irq_nop,
                     CODE64_SEG,
                     IDTET_IRQ,
                     0);
    }
    idt_set_gate(0x00, (uint64_t)(uintptr_t)isr0,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x01, (uint64_t)(uintptr_t)isr1,   CODE64_SEG, IDTET_TRAP, 0);
    idt_set_gate(0x02, (uint64_t)(uintptr_t)isr2,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x03, (uint64_t)(uintptr_t)isr3,   CODE64_SEG, IDTET_TRAP, 3); 
    idt_set_gate(0x04, (uint64_t)(uintptr_t)isr4,   CODE64_SEG, IDTET_TRAP, 0);
    idt_set_gate(0x05, (uint64_t)(uintptr_t)isr5,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x06, (uint64_t)(uintptr_t)isr6,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x07, (uint64_t)(uintptr_t)isr7,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x08, (uint64_t)(uintptr_t)isr8,   CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x0D, (uint64_t)(uintptr_t)isr13,  CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x0E, (uint64_t)(uintptr_t)isr14,  CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(0x10, (uint64_t)(uintptr_t)isr16,  CODE64_SEG, IDTET_IRQ,  0);
    idt_set_gate(I_TIMER,    (uint64_t)(uintptr_t)irq0,  CODE64_SEG, IDTET_IRQ, 0);
    idt_set_gate(I_KEYBOARD, (uint64_t)(uintptr_t)irq1,  CODE64_SEG, IDTET_IRQ, 0);
    idt_set_gate(I_MOUSE,    (uint64_t)(uintptr_t)irq12, CODE64_SEG, IDTET_IRQ, 0);
    idt_ptr.limit = (uint16_t)(sizeof(idt_gate_t) * IDT_ENTRIES - 1);
    idt_ptr.base  = (uint64_t)(uintptr_t)idt;
    idt_set_gate(I_SYSCALL, (uint64_t)(uintptr_t)isr_syscall,
      CODE64_SEG, IDTET_TRAP, 0);
    lidt(&idt_ptr);
}
