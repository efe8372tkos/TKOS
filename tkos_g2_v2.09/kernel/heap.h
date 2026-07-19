#pragma once
#include "types.h"

/*
 * TKOS - Genel Amacli Heap Allocator (byte granulariteli)
 * TempleOS Kernel/Mem/MAllocFree.HC (_MALLOC/_FREE/_MSIZE) mantigindan
 * uyarlanmistir.
 *
 * alloc.c (bitmap sayfa allocator, 4KB granularite) UZERINE kuruludur;
 * TempleOS'taki MemPagTaskAlloc/MemPagTaskFree karsiligi olarak
 * alloc_pages_raw()/free_pages() kullanilir.
 *
 * Algoritma (TempleOS MAlloc ile birebir ayni fikir):
 *   1. Kucuk tahsis + tam boy (exact-size) hash bucket'ta hazir parca
 *      varsa: O(1) hizli yol.
 *   2. Kucuk tahsis + hash miss: genel free-list'te gez, uyan ilk
 *      parcayi bul. Parca daha buyukse UST kismindan (yuksek adres)
 *      kadar kadar kes; alt kisim (kucultulmus) free listede kalir.
 *      Boylece free node'un adresi/next pointer'i degismez, sadece
 *      size alani kucultulur - relink gerekmez.
 *   3. Genel liste tukendiyse: OS'tan yeni bir sayfa blogu (>=16 sayfa)
 *      al, ayni "ustten kes" mantigiyla kullan, kalanini genel listeye
 *      ekle.
 *   4. Buyuk tahsis (>= MEM_HEAP_HASH_SIZE): dogrudan kendi sayfa
 *      blogunu alir, tamamini kullanir (TempleOS ile ayni).
 *
 * TempleOS'tan BILINCLI OLARAK CIKARILAN kisimlar (basitlik icin):
 *   - Cok-gorevli (per-task) heap / CTask baglantisi: TEK global kernel
 *     heap'i (kheap) var. Ileride gorev basina heap gerekirse
 *     heap_ctrl_t COGALTILARAK kolayca eklenebilir.
 *   - HeapLog / caller1-caller2 debug izleme (_CFG_HEAP_DBG).
 *   - MAllocAligned / StrNew / MAllocIdent gibi yardimci sarmalayicilar
 *     (gerekirse kmalloc uzerine ayrica eklenebilir).
 *   - last_mergable ile ardisik buyume bloklarini birlestirme
 *     optimizasyonu: kucuk bir fragmantasyon optimizasyonu oldugu icin
 *     dogruluk acisindan kritik degil, atlandi.
 *   - Buyume sirasinda "kalan parca sizeof(mem_unused_t)'tan kucuk"
 *     nadir kenar durumunda TempleOS listede yeniden arar; biz basitce
 *     tum blogu o tahsise veririz (birkac byte'lik israf, guvenli).
 *
 * Onemli davranis notu (TempleOS ile ayni): heap sadece BUYUR, kucuk
 * tahsisler icin alinan sayfa bloklari OS'a asla geri verilmez ("Never
 * Free these chunks" - orijinal yorum). Sadece >= MEM_HEAP_HASH_SIZE
 * olan "buyuk" tahsisler kfree() ile sayfa allocator'ina geri doner.
 */

/* Tam-boy (exact-size) hizli free-list sayisi. Bu deger ALTINDAKI
 * her byte boyutu icin ayri bir bucket vardir (heap_hash[size]).
 * Bu deger ve USTU boyutlar "buyuk tahsis" (dogrudan sayfa blogu)
 * olarak islenir. */
#define MEM_HEAP_HASH_SIZE   512

/* Heap buyume adimi: her seferinde en az bu kadar sayfa istenir
 * (TempleOS: 16*MEM_PAG_SIZE). */
#define MEM_HEAP_GROW_PAGES  16

/* Heap kontrol blogu imza degeri - gecerlilik kontrolu icin. */
#define HEAP_CTRL_SIGNATURE_VAL   0x4B4F5348UL  /* "KOSH" (ASCII, 32-bit) */

/*
 * mem_unused_t - Bos (kullanilmayan) parca basligi.
 * TempleOS CMemUnused karsiligi.
 * Free-list dugumlerinde bu yapi parcanin en basina yazilir.
 */
typedef struct mem_unused {
    struct mem_unused *next;   /* Sonraki bos parca (tek yonlu liste)   */
    uint64_t            size;  /* Bu parcanin TOPLAM boyutu (baslik dahil) */
} mem_unused_t;

/*
 * mem_used_t - Kullanimda olan parca basligi.
 * TempleOS CMemUsed karsiligi.
 * kmalloc()'un donderdigi pointer, bu yapinin HEMEN ardindan baslar.
 * sizeof(mem_used_t) == sizeof(mem_unused_t) olmasi (ikisi de 16 byte)
 * kritik bir invaryanttir: kfree() sirasinda bir mem_used_t, oldugu
 * yerde mem_unused_t olarak yeniden yorumlanir.
 */
typedef struct mem_used {
    uint64_t          size;  /* Toplam boyut (baslik dahil)      */
    struct heap_ctrl *hc;    /* Ait oldugu heap kontrol blogu     */
} mem_used_t;

/*
 * mem_blk_t - Ham sayfa blogu basligi.
 * TempleOS CMemBlk karsiligi (sadelestirilmis: sadece pags alani).
 * alloc_pages_raw() ile alinan HER ham blogun basina yazilir; boylece
 * kfree() sirasinda (buyuk tahsislerde) kac sayfa geri verilecegi
 * bilinir.
 */
typedef struct mem_blk {
    uint64_t pags;    /* Blogun kapladigi sayfa sayisi (PAGE_SIZE birimi) */
} mem_blk_t;

/*
 * heap_ctrl_t - Heap kontrol blogu.
 * TempleOS CHeapCtrl karsiligi (sadelestirilmis).
 */
typedef struct heap_ctrl {
    uint32_t            hc_signature;   /* HEAP_CTRL_SIGNATURE_VAL olmali */
    volatile uint32_t   locked_flags;   /* 0=acik, 1=kilitli (spinlock)   */

    uint64_t            used_u8s;       /* Su an kullanimdaki byte        */
    uint64_t            total_u8s;      /* OS'tan simdiye kadar alinan byte */

    mem_unused_t       *malloc_free_lst; /* Genel (tam-boy olmayan) free liste */
    mem_unused_t       *heap_hash[MEM_HEAP_HASH_SIZE]; /* Tam-boy free listeleri */
} heap_ctrl_t;

/* Global cekirdek heap'i (simdilik tek heap - "Adam's heap" karsiligi). */
extern heap_ctrl_t kheap;

/* heap_init() - Heap kontrol blogunu sifirlar ve imzayi yazar.
 * alloc_init()'ten SONRA cagrilmalidir (alloc_pages_raw'a bagimli). */
void heap_init(heap_ctrl_t *hc);

/* kheap_init() - heap_init(&kheap) kisayolu. Global cekirdek heap'ini
 * baslatmak icin kernel_main.c'den cagrilir. */
void kheap_init(void);

/* Basit sayac okuyuculari (heap_ctrl_t struct'ini gormeye gerek
 * kalmadan istatistik icin kullanilabilir). */
uint64_t heap_get_used(void);
uint64_t heap_get_total(void);

/* kmalloc() - TempleOS MAlloc() karsiligi. Basarisizsa NULL doner. */
void *kmalloc(uint64_t size);

/* kcalloc() - TempleOS CAlloc() karsiligi: kmalloc + sifirlama. */
void *kcalloc(uint64_t size);

/* kfree() - TempleOS Free() karsiligi. NULL guvenlidir (no-op). */
void kfree(void *ptr);

/* ksize() - TempleOS MSize() karsiligi: kullaniciya gorunen boyut
 * (baslik haric). ptr NULL ise 0 doner. */
uint64_t ksize(void *ptr);

/* heap_print_stats() - Debug/istatistik ciktisi (alloc_print_stats
 * ile ayni tarzda). */
void heap_print_stats(void);
