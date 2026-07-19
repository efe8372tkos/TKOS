#pragma once
#include "types.h"

/*
 * TKOS - Bitmap Sayfa Allocator
 * TempleOS CAlloc/MAlloc mantigindan uyarlanmistir.
 *
 * Calisma prensibi:
 *   - Fiziksel bellek 4KB sayfalar halinde yonetilir.
 *   - Her sayfa icin bitmap'te 1 bit tutulur (0=bos, 1=dolu).
 *   - CAlloc karsiligi: alloc_pages() -> sifirlanmis bellek
 *   - MAlloc karsiligi: alloc_pages_raw() -> ham bellek
 *   - Free karsiligi:   free_pages()
 *
 * Bellek duzeni (stage2.asm ile uyumlu):
 *   0x0000 - 0x7FFF  : BIOS / IVT / BDA
 *   0x7C00 - 0x7DFF  : Stage1 MBR
 *   0x7E00 - 0x9FFF  : Stage2
 *   0x9000           : VBE bilgi blogu
 *   0x10000- 0x15FFF : Sayfa tablolari (PML4/PDPT/PD)
 *   0x200000+        : Kernel
 *   0x290000         : Kernel stack
 *   0x300000+        : Heap baslangici (HEAP_START)
 *
 * Bitmap kendisi heap baslangicinda yer alir.
 * Maksimum 4GB / 4KB = 1M sayfa = 128KB bitmap.
 */

/* Sayfa boyutu: 4KB */
#define PAGE_SIZE       4096
#define PAGE_SHIFT      12

/* Heap baslangici (kernel + stack'in ustunden) */
#define HEAP_START      0x400000UL      /* 4MB */
#define HEAP_END        0x40000000UL    /* 1GB (ilk asama) */

/* Toplam yonetilebilir sayfa sayisi */
#define TOTAL_PAGES     ((HEAP_END - HEAP_START) / PAGE_SIZE)

/* Bitmap boyutu (byte cinsinden) */
#define BITMAP_SIZE     (TOTAL_PAGES / 8)

/*
 * Allocator durum yapisi.
 * TempleOS CHeapCtrl'den esinlenilmistir.
 */
typedef struct {
    uint8_t  *bitmap;           /* Bit dizisi: 0=bos, 1=dolu  */
    uint64_t  total_pages;      /* Toplam sayfa sayisi         */
    uint64_t  used_pages;       /* Kullanilan sayfa sayisi     */
    uint64_t  base_addr;        /* Yonetilebilir alan baslangici */
    uint32_t  last_alloc_idx;   /* Son tahsis edilen sayfa idx */
} alloc_state_t;

extern alloc_state_t kalloc_state;

/*
 * alloc_init() - Allocator'u baslatir.
 * Bitmap'i sifirlar, rezerv alanlari isaretle.
 * TempleOS BlkPoolsInit + Mem32DevInit mantigindan
 * esinlenilmistir.
 */
void alloc_init(void);

/*
 * alloc_pages() - N adet ardisik sayfa tahsis eder, sifirlar.
 * TempleOS CAlloc(size) karsiligi.
 * Basarisizsa NULL dondurur.
 *
 * @n : istenen ardisik sayfa sayisi
 */
void *alloc_pages(uint64_t n);

/*
 * alloc_pages_raw() - N adet ardisik sayfa tahsis eder, sifirlamaz.
 * TempleOS MAlloc(size) karsiligi.
 * Basarisizsa NULL dondurur.
 *
 * @n : istenen ardisik sayfa sayisi
 */
void *alloc_pages_raw(uint64_t n);

/*
 * free_pages() - Tahsis edilmis sayfalari serbest birakir.
 * TempleOS Free() karsiligi.
 *
 * @ptr : alloc_pages/alloc_pages_raw'dan donen adres
 * @n   : serbest birakilacak sayfa sayisi
 */
void free_pages(void *ptr, uint64_t n);

/*
 * alloc_page() - Tek sayfa tahsis eder (sifirlanmis).
 * Kisayol: alloc_pages(1)
 */
static inline void *alloc_page(void) {
    return alloc_pages(1);
}

/*
 * free_page() - Tek sayfayi serbest birakir.
 * Kisayol: free_pages(ptr, 1)
 */
static inline void free_page(void *ptr) {
    free_pages(ptr, 1);
}

/*
 * alloc_get_free() - Bos sayfa sayisini dondurur.
 */
uint64_t alloc_get_free(void);

/*
 * alloc_get_used() - Kullanilan sayfa sayisini dondurur.
 */
uint64_t alloc_get_used(void);

/*
 * alloc_print_stats() - Bellek istatistiklerini konsola yazar.
 * Debug amaciyla.
 */
void alloc_print_stats(void);
