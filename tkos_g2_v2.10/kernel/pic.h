#pragma once
#include "types.h"



















#define PIC1_CMD    0x20    
#define PIC1_DATA   0x21    
#define PIC2_CMD    0xA0    
#define PIC2_DATA   0xA1    


#define PIC_EOI     0x20


#define PIC_ICW1_ICW4   0x01    
#define PIC_ICW1_INIT   0x10    
#define PIC_ICW4_8086   0x01    
#define PIC_ICW4_AEOI   0x02    
#define PIC_ICW4_SFNM   0x10    


#define PIC1_OFFSET 0x20        
#define PIC2_OFFSET 0x28        


#define IRQ_TIMER    0
#define IRQ_KEYBOARD 1
#define IRQ_CASCADE  2
#define IRQ_RTC      8
#define IRQ_MOUSE    12
#define IRQ_ATA1     14
#define IRQ_ATA2     15










void pic_init(void);





void pic_send_eoi(uint8_t irq);





void pic_mask_irq(uint8_t irq);
void pic_unmask_irq(uint8_t irq);





uint16_t pic_get_mask(void);





void pic_disable(void);
