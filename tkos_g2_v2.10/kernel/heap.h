#pragma once
#include "types.h"
















































#define MEM_HEAP_HASH_SIZE   512



#define MEM_HEAP_GROW_PAGES  16


#define HEAP_CTRL_SIGNATURE_VAL   0x4B4F5348UL  






typedef struct mem_unused {
    struct mem_unused *next;   
    uint64_t            size;  
} mem_unused_t;









typedef struct mem_used {
    uint64_t          size;  
    struct heap_ctrl *hc;    
} mem_used_t;








typedef struct mem_blk {
    uint64_t pags;    
} mem_blk_t;





typedef struct heap_ctrl {
    uint32_t            hc_signature;   
    volatile uint32_t   locked_flags;   

    uint64_t            used_u8s;       
    uint64_t            total_u8s;      

    mem_unused_t       *malloc_free_lst; 
    mem_unused_t       *heap_hash[MEM_HEAP_HASH_SIZE]; 
} heap_ctrl_t;


extern heap_ctrl_t kheap;



void heap_init(heap_ctrl_t *hc);



void kheap_init(void);



uint64_t heap_get_used(void);
uint64_t heap_get_total(void);


void *kmalloc(uint64_t size);


void *kcalloc(uint64_t size);


void kfree(void *ptr);



uint64_t ksize(void *ptr);



void heap_print_stats(void);
