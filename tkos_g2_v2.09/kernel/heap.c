#include "heap.h"
#include "alloc.h"
#include "fb_console.h"
#include "string.h"
#include "types.h"

/*
 * TKOS - Genel Amacli Heap Allocator Implementasyonu
 * TempleOS Kernel/Mem/MAllocFree.HC (_MALLOC/_FREE/_MSIZE) mantigindan
 * uyarlanmistir. Detayli aciklama icin heap.h basindaki yorumlara bakiniz.
 */

heap_ctrl_t kheap;

/* ------------------------------------------------
 * Kilitleme yardimcilari
 *
 * TempleOS: PUSHFD + CLI ... BTS/PAUSE spin-lock ... POPFD
 * Biz de ayni ikili yapiyi koruyoruz:
 *   1) irq_save_cli()/irq_restore()  -> RFLAGS'i sakla, kesmeleri kapat
 *      (tek cekirdek oldugumuz icin asil koruma budur)
 *   2) heap_spin_lock()/unlock()     -> ileride cok cekirdek (SMP)
 *      destegi eklenirse hazir olsun diye BTS tarzi spinlock
 * ------------------------------------------------ */
#ifndef HEAP_HOST_TEST
static inline uint64_t irq_save_cli(void) {
    uint64_t flags;
    __asm__ volatile (
        "pushfq\n\t"
        "pop %0\n\t"
        "cli"
        : "=r"(flags) :: "memory");
    return flags;
}

static inline void irq_restore(uint64_t flags) {
    __asm__ volatile (
        "push %0\n\t"
        "popfq"
        :: "r"(flags) : "memory", "cc");
}
#else
/* HEAP_HOST_TEST: pushfq/cli/popfq Ring0 ayricalikli komutlardir, host
 * (kullanici modu) test binary'sinde calismaz -> zararsiz yer tutucu.
 * Gercek kernel derlemesinde bu blok DEVREYE GIRMEZ. */
static inline uint64_t irq_save_cli(void) { return 0; }
static inline void irq_restore(uint64_t flags) { (void)flags; }
#endif

static inline void heap_spin_lock(heap_ctrl_t *hc) {
    while (__sync_lock_test_and_set(&hc->locked_flags, 1)) {
        __asm__ volatile ("pause");
    }
}

static inline void heap_spin_unlock(heap_ctrl_t *hc) {
    __sync_lock_release(&hc->locked_flags);
}

/* ------------------------------------------------
 * heap_panic() - Bozuk heap tespit edilirse.
 * TempleOS SysBadMAlloc/SysBadFree + _SYS_HLT karsiligi.
 * ------------------------------------------------ */
static void heap_panic(const char *msg) {
    con_set_color(12, 0); /* Kirmizi */
    con_print("\n\n*** TKOS HEAP HATASI ***\n");
    con_print(msg);
    con_print("\nSistem Durduruldu.\n");
    for (;;) __asm__ volatile ("cli\n\thlt");
}

/* ------------------------------------------------
 * heap_init()
 * TempleOS heap kurulumu (Core0Init icindeki HeapCtrl init) karsiligi.
 * ------------------------------------------------ */
void heap_init(heap_ctrl_t *hc) {
    memset(hc, 0, sizeof(heap_ctrl_t));
    hc->hc_signature = HEAP_CTRL_SIGNATURE_VAL;
}

void kheap_init(void) {
    heap_init(&kheap);
}

uint64_t heap_get_used(void)  { return kheap.used_u8s;  }
uint64_t heap_get_total(void) { return kheap.total_u8s; }

/* ------------------------------------------------
 * kmalloc()
 * TempleOS _MALLOC ile ayni algoritma. Detayli akis icin heap.h'a
 * ve fonksiyon icindeki adim yorumlarina bakiniz.
 * ------------------------------------------------ */
void *kmalloc(uint64_t size) {
    heap_ctrl_t *hc = &kheap;

    if (hc->hc_signature != HEAP_CTRL_SIGNATURE_VAL)
        heap_panic("Heap baslatilmamis (heap_init cagrilmadi)");

    if (size == 0)
        size = 1;

    /*
     * Toplam boyut: kullanici verisi + baslik, 8 byte'a yuvarla.
     * TempleOS: ADD RAX,CMemUsed.start+7; AND AL,0xF8
     */
    uint64_t total = size + sizeof(mem_used_t);
    total = (total + 7) & ~7ULL;
    if (total < sizeof(mem_unused_t))
        total = sizeof(mem_unused_t);  /* Serbest kalinca free-node sigmali */

    uint64_t flags = irq_save_cli();
    heap_spin_lock(hc);

    mem_used_t *res;

    if (total < MEM_HEAP_HASH_SIZE) {
        /* =========================================================
         * KUCUK TAHSIS
         * ========================================================= */

        /* ---- 1) Hizli yol: tam-boy (exact-size) hash bucket ---- */
        mem_unused_t *hit = hc->heap_hash[total];
        if (hit) {
            hc->heap_hash[total] = hit->next;
            res = (mem_used_t *)hit;
            goto almost_done;
        }

        /* ---- 2) Genel free-list'te ara (ilk-uyan / top-carve) ---- */
        mem_unused_t *prev = NULL;
        mem_unused_t *cur  = hc->malloc_free_lst;

        while (cur) {
            if (cur->size == total) {
                /* Tam uyum: listeden cikar (unlink), oldugu gibi kullan */
                if (prev) prev->next = cur->next;
                else      hc->malloc_free_lst = cur->next;
                res = (mem_used_t *)cur;
                goto almost_done;
            }

            if (cur->size > total) {
                uint64_t remain = cur->size - total;
                if (remain >= sizeof(mem_unused_t)) {
                    /*
                     * Parcayi UST (yuksek adresli) ucundan kes:
                     *   - alt kisim (cur, kucultulmus) free listede
                     *     AYNI ADRESTE kalir -> relink gerekmez
                     *   - ust kisim yeni kullanim parcasi olur
                     */
                    cur->size = remain;
                    res = (mem_used_t *)((uint8_t *)cur + remain);
                    goto almost_done;
                }
                /* Sigmiyor (kalan cok kucuk): bu parcayi atla, aramaya
                 * devam et. */
            }

            prev = cur;
            cur  = cur->next;
        }

        /* ---- 3) Liste tukendi: OS'tan yeni sayfa blogu al (buyu) ---- */
        uint64_t grow_pages =
            (total + MEM_HEAP_GROW_PAGES * PAGE_SIZE - 1) / PAGE_SIZE;

        mem_blk_t *blk = (mem_blk_t *)alloc_pages_raw(grow_pages);
        if (!blk) {
            heap_spin_unlock(hc);
            irq_restore(flags);
            return NULL;   /* Bellek yetersiz (TempleOS: throw 'OutMem') */
        }
        blk->pags = grow_pages;
        hc->total_u8s += grow_pages * PAGE_SIZE;

        uint64_t block_bytes = grow_pages * PAGE_SIZE - sizeof(mem_blk_t);
        uint8_t *chunk       = (uint8_t *)blk + sizeof(mem_blk_t);
        uint64_t remain      = block_bytes - total;

        if (remain >= sizeof(mem_unused_t)) {
            /* Kalani genel free listenin basina ekle, ustten total al */
            mem_unused_t *leftover = (mem_unused_t *)chunk;
            leftover->size = remain;
            leftover->next = hc->malloc_free_lst;
            hc->malloc_free_lst = leftover;
            res = (mem_used_t *)(chunk + remain);
        } else {
            /* Kalan cok kucuk (nadir kenar durumu): tum blogu bu
             * tahsise ver (birkac byte israf, ama basit ve guvenli) */
            total = block_bytes;
            res = (mem_used_t *)chunk;
        }
    } else {
        /* =========================================================
         * BUYUK TAHSIS - dogrudan kendi sayfa blogu
         * ========================================================= */
        uint64_t pages =
            (total + sizeof(mem_blk_t) + PAGE_SIZE - 1) / PAGE_SIZE;

        mem_blk_t *blk = (mem_blk_t *)alloc_pages_raw(pages);
        if (!blk) {
            heap_spin_unlock(hc);
            irq_restore(flags);
            return NULL;
        }
        blk->pags = pages;
        hc->total_u8s += pages * PAGE_SIZE;

        total = pages * PAGE_SIZE - sizeof(mem_blk_t);
        res = (mem_used_t *)((uint8_t *)blk + sizeof(mem_blk_t));
    }

almost_done:
    hc->used_u8s += total;
    res->size = total;
    res->hc   = hc;

    heap_spin_unlock(hc);
    irq_restore(flags);

    return (void *)(res + 1);   /* Baslik hemen sonrasi = kullanici verisi */
}

/* ------------------------------------------------
 * kcalloc()
 * TempleOS CAlloc(): MAlloc + MemSet(0)
 * ------------------------------------------------ */
void *kcalloc(uint64_t size) {
    void *ptr = kmalloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

/* ------------------------------------------------
 * kfree()
 * TempleOS _FREE ile ayni algoritma:
 *   - kucuk parca (< MEM_HEAP_HASH_SIZE) -> tam-boy hash bucket'a it
 *   - buyuk parca                        -> sayfa blogu OS'a geri doner
 * ------------------------------------------------ */
void kfree(void *ptr) {
    if (!ptr) return;

    mem_used_t *used = (mem_used_t *)ptr - 1;
    heap_ctrl_t *hc  = used->hc;

    if (!hc || hc->hc_signature != HEAP_CTRL_SIGNATURE_VAL)
        heap_panic("Gecersiz Free() cagrisi (bozuk pointer/heap)");

    uint64_t size = used->size;

    uint64_t flags = irq_save_cli();
    heap_spin_lock(hc);

    hc->used_u8s -= size;

    if (size < MEM_HEAP_HASH_SIZE) {
        /* Kucuk parca: tam-boy hash bucket'in basina it (LIFO) */
        mem_unused_t *node = (mem_unused_t *)used;
        node->size = size;
        node->next = hc->heap_hash[size];
        hc->heap_hash[size] = node;
    } else {
        /* Buyuk parca: tum sayfa blogunu OS'a geri ver.
         * (TempleOS yorumu: "Never Free" kucuk-tahsis buyume
         *  bloklari icin gecerlidir; sadece buyuk/direkt bloklar
         *  boyle geri doner.) */
        mem_blk_t *blk = (mem_blk_t *)((uint8_t *)used - sizeof(mem_blk_t));
        uint64_t   pages = blk->pags;
        free_pages(blk, pages);
        hc->total_u8s -= pages * PAGE_SIZE;
    }

    heap_spin_unlock(hc);
    irq_restore(flags);
}

/* ------------------------------------------------
 * ksize()
 * TempleOS _MSIZE karsiligi (aligned-alloc negatif-boyut hilesi
 * portlanmadi; bkz. heap.h basi).
 * ------------------------------------------------ */
uint64_t ksize(void *ptr) {
    if (!ptr) return 0;
    mem_used_t *used = (mem_used_t *)ptr - 1;
    return used->size - sizeof(mem_used_t);
}

/* ------------------------------------------------
 * heap_print_stats() - Debug ciktisi (alloc_print_stats ile ayni tarz)
 * ------------------------------------------------ */
void heap_print_stats(void) {
    heap_ctrl_t *hc = &kheap;

    con_print("[HEAP] Kullanimda : ");
    con_print_dec(hc->used_u8s);
    con_print(" byte\n");

    con_print("[HEAP] OS'tan alinan (toplam) : ");
    con_print_dec(hc->total_u8s);
    con_print(" byte (");
    con_print_dec(hc->total_u8s / PAGE_SIZE);
    con_print(" sayfa)\n");

    con_print("[HEAP] Imza   : ");
    con_print_hex(hc->hc_signature);
    con_print("\n");
}
