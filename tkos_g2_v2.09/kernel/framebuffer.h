#pragma once
#include "types.h"

/*
 * stage2 tarafindan 0x9000'e yazilan VBE bilgi blogu:
 *   [0..3] uint32  fb_phys_addr
 *   [4..5] uint16  fb_pitch       (BytesPerScanLine)
 *   [6]    uint8   fb_bpp         (BitsPerPixel)
 *   [7]    uint8   ok             (1 = VBE aktif)
 */

int  fb_init(void);                                        /* 1 = basarili, 0 = VBE yok */
void fb_put_pixel(uint32_t x, uint32_t y, uint8_t color); /* 8bpp palet indeksi */
uint8_t fb_get_pixel(uint32_t x, uint32_t y);              /* mouse cursor save/restore icin */
void fb_fill(uint8_t color);                               /* tum ekrani doldur  */
void fb_fill_rect(uint32_t x, uint32_t y,
                  uint32_t w, uint32_t h, uint8_t color);

/* Sorgu fonksiyonlari */
uint32_t fb_width(void);
uint32_t fb_height(void);
uint8_t  fb_bpp(void);
int      fb_ready(void);  /* fb_init basarili olduysa 1 */
