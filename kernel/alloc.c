#include "alloc.h"
#include "fb_console.h"
#include "string.h"
#include "types.h"
alloc_state_t kalloc_state;
static uint8_t *bitmap_base = (uint8_t *)HEAP_START;
#define PAGES_START  (HEAP_START + BITMAP_SIZE)
static inline void bitmap_set(uint32_t idx) {
    bitmap_base[idx >> 3] |= (1 << (idx & 7));
}
static inline void bitmap_clear(uint32_t idx) {
    bitmap_base[idx >> 3] &= ~(1 << (idx & 7));
}
static inline int bitmap_test(uint32_t idx) {
    return (bitmap_base[idx >> 3] >> (idx & 7)) & 1;
}
void alloc_init(void) {
    uint64_t i;
    uint64_t bitmap_pages;
    uint64_t reserved_end_page;
    kalloc_state.bitmap       = bitmap_base;
    kalloc_state.base_addr    = PAGES_START;
    kalloc_state.used_pages   = 0;
    kalloc_state.last_alloc_idx = 0;
    memset(bitmap_base, 0, BITMAP_SIZE);
    kalloc_state.total_pages = (HEAP_END - PAGES_START) / PAGE_SIZE;
    bitmap_pages = (BITMAP_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    reserved_end_page = bitmap_pages;
    for (i = 0; i < reserved_end_page && i < kalloc_state.total_pages; i++) {
        bitmap_set((uint32_t)i);
        kalloc_state.used_pages++;
    }
}
static int32_t find_free_pages(uint64_t n) {
    uint64_t total = kalloc_state.total_pages;
    uint64_t start = kalloc_state.last_alloc_idx;
    uint64_t found = 0;
    uint64_t i, j;
    for (i = start; i < total; i++) {
        if (!bitmap_test((uint32_t)i)) {
            if (found == 0) j = i;  
            found++;
            if (found == n) return (int32_t)j;
        } else {
            found = 0;
        }
    }
    found = 0;
    for (i = 0; i < start; i++) {
        if (!bitmap_test((uint32_t)i)) {
            if (found == 0) j = i;
            found++;
            if (found == n) return (int32_t)j;
        } else {
            found = 0;
        }
    }
    return -1;  
}
void *alloc_pages_raw(uint64_t n) {
    int32_t  idx;
    uint64_t i;
    void    *addr;
    if (n == 0) return NULL;
    if (kalloc_state.used_pages + n > kalloc_state.total_pages)
        return NULL;
    idx = find_free_pages(n);
    if (idx < 0) return NULL;
    for (i = 0; i < n; i++) {
        bitmap_set((uint32_t)(idx + i));
    }
    kalloc_state.used_pages   += n;
    kalloc_state.last_alloc_idx = (uint32_t)(idx + n) %
                                   (uint32_t)kalloc_state.total_pages;
    addr = (void *)(uintptr_t)(kalloc_state.base_addr +
                                (uint64_t)idx * PAGE_SIZE);
    return addr;
}
void *alloc_pages(uint64_t n) {
    void *ptr = alloc_pages_raw(n);
    if (ptr) {
        memset(ptr, 0, n * PAGE_SIZE);
    }
    return ptr;
}
void free_pages(void *ptr, uint64_t n) {
    uint64_t addr = (uint64_t)(uintptr_t)ptr;
    uint64_t idx;
    uint64_t i;
    if (!ptr || n == 0) return;
    if (addr < kalloc_state.base_addr || addr >= HEAP_END) return;
    idx = (addr - kalloc_state.base_addr) / PAGE_SIZE;
    for (i = 0; i < n; i++) {
        if (idx + i < kalloc_state.total_pages) {
            bitmap_clear((uint32_t)(idx + i));
        }
    }
    if (kalloc_state.used_pages >= n)
        kalloc_state.used_pages -= n;
    else
        kalloc_state.used_pages = 0;
    if ((uint32_t)idx < kalloc_state.last_alloc_idx)
        kalloc_state.last_alloc_idx = (uint32_t)idx;
}
uint64_t alloc_get_free(void) {
    return kalloc_state.total_pages - kalloc_state.used_pages;
}
uint64_t alloc_get_used(void) {
    return kalloc_state.used_pages;
}
void alloc_print_stats(void) {
    uint64_t free_pages_count = alloc_get_free();
    uint64_t used_pages_count = alloc_get_used();
    con_print("[MEM] Total : ");
    con_print_dec(kalloc_state.total_pages * PAGE_SIZE / 1024);
    con_print(" KB\n");
    con_print("[MEM] Used  : ");
    con_print_dec(used_pages_count * PAGE_SIZE / 1024);
    con_print(" KB (");
    con_print_dec(used_pages_count);
    con_print(" pages)\n");
    con_print("[MEM] Free  : ");
    con_print_dec(free_pages_count * PAGE_SIZE / 1024);
    con_print(" KB (");
    con_print_dec(free_pages_count);
    con_print(" pages)\n");
    con_print("[MEM] Base  : ");
    con_print_hex(kalloc_state.base_addr);
    con_print("\n");
}
