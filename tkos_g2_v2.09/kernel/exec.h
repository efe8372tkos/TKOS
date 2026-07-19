
#pragma once
#include "types.h"
#include "fat16.h"
#include "task.h"

/*
 * TKOS - TKX Calistirilabilir Dosya Formati ve Yukleyici
 *
 * TKX ("TKOS eXecutable"): diskten yuklenip gorev olarak
 * calistirilacak minimal, ozel bir ikili format:
 *
 *   [tkx_header_t - 32 byte]
 *   [image - image_size byte, entry_offset bu bolgeye gore]
 *
 * ONEMLI - KONUM-BAGIMSIZ KOD (PIC) ZORUNLU:
 * image_base her calistirmada FARKLI bir adres olabilir (alloc_pages
 * first-fit sayfa allocator'indan gelir, sabit degildir). TKX
 * imajlari SADECE RIP-relative adresleme kullanmali - NASM'da
 * 'default rel' + 'lea reg, [rel etiket]' ile saglanir. Mutlak
 * adres referansi iceren kod yanlis calisir/coker.
 */

#define TKX_MAGIC     0x31584B54UL   /* "TKX1" (little-endian ASCII) */

typedef struct __attribute__((packed)) {
    uint32_t magic;         /* TKX_MAGIC olmali                           */
    uint32_t version;       /* format versiyonu (simdilik 1)              */
    uint64_t entry_offset;  /* image basindan itibaren giris noktasi      */
    uint64_t image_size;    /* header sonrasi kopyalanacak kod+veri (byte)*/
    uint64_t stack_size;    /* istenen yigin boyutu (byte); 0=varsayilan  */
} tkx_header_t;

typedef enum {
    EXEC_OK              =  0,
    EXEC_ERR_NOT_FOUND   = -1,
    EXEC_ERR_IS_DIR      = -2,
    EXEC_ERR_TOO_SMALL   = -3,
    EXEC_ERR_NOMEM       = -4,
    EXEC_ERR_IO          = -5,
    EXEC_ERR_BAD_MAGIC   = -6,
    EXEC_ERR_BAD_ENTRY   = -7,
    EXEC_ERR_TRUNCATED   = -8,
    EXEC_ERR_TASK        = -9,
} exec_result_t;

/*
 * exec_load() - TKX dosyasini okur, dogrular, bellege yukler ve
 * yeni bir GOREV olarak HAZIRLAR (henuz zamanlayiciya EKLEMEZ -
 * basarili donuste cagiran tarafin sched_add(*out_task) cagirmasi
 * gerekir; bkz. kernel_main.c shell_cmd_exec()).
 *
 * Donus: EXEC_OK (0) basari, aksi halde exec_result_t hata kodu.
 */
int exec_load(fat16_volume_t *vol, const char *filename, task_t **out_task);

/* exec_strerror() - hata kodunu okunabilir mesaja cevirir. */
const char *exec_strerror(int result);
