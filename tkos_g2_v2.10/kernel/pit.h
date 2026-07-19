#pragma once
#include "types.h"
#include "idt.h"




















#define PIT_CH0     0x40    
#define PIT_CH1     0x41    
#define PIT_CH2     0x42    
#define PIT_CMD     0x43    


#define PIT_BASE_FREQ   1193182UL


#define PIT_HZ          1000        






#define PIT_DIVISOR     (PIT_BASE_FREQ / PIT_HZ)












#define PIT_CMD_CH0_RATE    0x34    
#define PIT_CMD_CH2_SQUARE  0xB6    


extern volatile uint64_t pit_ticks;










void pit_init(void);






uint64_t pit_get_ticks(void);








void pit_sleep_ms(uint64_t ms);







void pit_irq_handler(int_frame_t *frame);
