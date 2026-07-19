#pragma once
#include "types.h"



























#define PAGE_SIZE       4096
#define PAGE_SHIFT      12


#define HEAP_START      0x400000UL      
#define HEAP_END        0x40000000UL    


#define TOTAL_PAGES     ((HEAP_END - HEAP_START) / PAGE_SIZE)


#define BITMAP_SIZE     (TOTAL_PAGES / 8)





typedef struct {
    uint8_t  *bitmap;           
    uint64_t  total_pages;      
    uint64_t  used_pages;       
    uint64_t  base_addr;        
    uint32_t  last_alloc_idx;   
} alloc_state_t;

extern alloc_state_t kalloc_state;







void alloc_init(void);








void *alloc_pages(uint64_t n);








void *alloc_pages_raw(uint64_t n);








void free_pages(void *ptr, uint64_t n);





static inline void *alloc_page(void) {
    return alloc_pages(1);
}





static inline void free_page(void *ptr) {
    free_pages(ptr, 1);
}




uint64_t alloc_get_free(void);




uint64_t alloc_get_used(void);





void alloc_print_stats(void);
