#pragma once
#include "types.h"















#define IDTET_TASK  0x05    
#define IDTET_IRQ   0x0E    
#define IDTET_TRAP  0x0F    


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
#define I_TIMER         0x20    
#define I_KEYBOARD      0x21    
#define I_MOUSE         0x2C    
#define I_MP_CRASH      0x30    
#define I_WAKE          0x31    
#define I_USER          0x40    
#define I_SYSCALL       0x80    


#define IDT_ENTRIES     256





















typedef struct __attribute__((packed)) {
    uint16_t offset_low;    
    uint16_t selector;      
    uint8_t  ist;           
    uint8_t  type_attr;     
    uint16_t offset_mid;    
    uint32_t offset_high;   
    uint32_t zero;          
} idt_gate_t;





typedef struct __attribute__((packed)) {
    uint16_t limit;         
    uint64_t base;          
} idt_ptr_t;






typedef struct __attribute__((packed)) {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} int_frame_t;


typedef void (*int_handler_t)(int_frame_t *frame);









void idt_init(void);













void idt_set_gate(uint8_t num, uint64_t handler,
                  uint16_t selector, uint8_t type, uint8_t dpl);





void idt_set_handler(uint8_t num, int_handler_t handler);








extern void isr0(void);   
extern void isr1(void);   
extern void isr2(void);   
extern void isr3(void);   
extern void isr4(void);   
extern void isr5(void);   
extern void isr6(void);   
extern void isr7(void);   
extern void isr8(void);   
extern void isr13(void);  
extern void isr14(void);  
extern void isr16(void);  
extern void irq0(void);   
extern void irq1(void);   
extern void irq12(void);  
extern void irq_nop(void);
extern void isr_syscall(void); 
