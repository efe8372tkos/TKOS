#include "exec.h"
#include "fat16.h"
#include "alloc.h"
#include "heap.h"
#include "task.h"
#include "string.h"
#include "types.h"
int exec_load(fat16_volume_t *vol, const char *filename, task_t **out_task) {
    if (!vol || !vol->mounted || !filename) return EXEC_ERR_NOT_FOUND;
    fat16_dirent_t dirent;
    if (!fat16_find_in_root(vol, filename, &dirent))
        return EXEC_ERR_NOT_FOUND;
    if (dirent.attr & FAT_ATTR_DIRECTORY)
        return EXEC_ERR_IS_DIR;
    if (dirent.size < sizeof(tkx_header_t))
        return EXEC_ERR_TOO_SMALL;
    uint8_t *filebuf = (uint8_t *)kmalloc(dirent.size);
    if (!filebuf) return EXEC_ERR_NOMEM;
    int n = fat16_read_file(vol, &dirent, filebuf, dirent.size);
    if (n < 0 || (uint32_t)n != dirent.size) {
        kfree(filebuf);
        return EXEC_ERR_IO;
    }
    tkx_header_t hdr;
    memcpy(&hdr, filebuf, sizeof(hdr));
    if (hdr.magic != TKX_MAGIC) {
        kfree(filebuf);
        return EXEC_ERR_BAD_MAGIC;
    }
    if (hdr.image_size == 0 || hdr.entry_offset >= hdr.image_size) {
        kfree(filebuf);
        return EXEC_ERR_BAD_ENTRY;
    }
    if ((uint64_t)sizeof(hdr) + hdr.image_size > dirent.size) {
        kfree(filebuf);
        return EXEC_ERR_TRUNCATED;
    }
    uint64_t img_pages = (hdr.image_size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *image_base = alloc_pages(img_pages);
    if (!image_base) {
        kfree(filebuf);
        return EXEC_ERR_NOMEM;
    }
    memcpy(image_base, filebuf + sizeof(hdr), hdr.image_size);
    kfree(filebuf);
    void (*entry_point)(void) =
        (void (*)(void))(uintptr_t)((uint8_t *)image_base + hdr.entry_offset);
    task_t *t = task_create(filename, entry_point, hdr.stack_size);
    if (!t) {
        free_pages(image_base, img_pages);
        return EXEC_ERR_TASK;
    }
    t->image_base  = image_base;
    t->image_pages = img_pages;
    if (out_task) *out_task = t;
    return EXEC_OK;
}
const char *exec_strerror(int result) {
    switch (result) {
        case EXEC_OK:            return "Basarili";
        case EXEC_ERR_NOT_FOUND: return "Dosya bulunamadi";
        case EXEC_ERR_IS_DIR:    return "Bu bir dizin, calistirilabilir dosya degil";
        case EXEC_ERR_TOO_SMALL: return "Dosya TKX basligindan kucuk";
        case EXEC_ERR_NOMEM:     return "Bellek yetersiz";
        case EXEC_ERR_IO:        return "Disk okuma hatasi";
        case EXEC_ERR_BAD_MAGIC: return "Gecersiz TKX imzasi (magic)";
        case EXEC_ERR_BAD_ENTRY: return "Gecersiz entry_offset/image_size";
        case EXEC_ERR_TRUNCATED: return "Dosyada image_size kadar veri yok (bozuk dosya)";
        case EXEC_ERR_TASK:      return "Gorev (task) olusturulamadi";
        default:                 return "Bilinmeyen hata";
    }
}
