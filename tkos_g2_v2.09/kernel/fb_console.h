#pragma once
#include "types.h"

/*
 * Framebuffer uzerine 8x8 bitmap font ile text render.
 * 640x480x8bpp icin: 80 sutun x 60 satir
 *
 * Palet indeksleri (set_palette ile uyumlu):
 *   0  = Siyah        8  = Koyu Gri
 *   1  = Koyu Mavi    9  = Mavi
 *   2  = Koyu Yesil   10 = Yesil
 *   3  = Koyu Cyan    11 = Cyan
 *   4  = Koyu Kirmizi 12 = Kirmizi
 *   5  = Koyu Magenta 13 = Magenta
 *   6  = Kahverengi   14 = Sari
 *   7  = Acik Gri     15 = Beyaz
 */

void con_init(uint8_t fg, uint8_t bg);   /* console baslatma, renk ayarla */
void con_clear(void);                    /* ekrani bg rengiyle temizle     */
void con_putchar(char c);                /* tek karakter yaz               */
void con_print(const char *str);         /* string yaz                     */
void con_print_at(uint32_t col, uint32_t row,
                  const char *str,
                  uint8_t fg, uint8_t bg); /* konumlu yazma               */
void con_set_color(uint8_t fg, uint8_t bg);
void con_set_cursor(uint32_t col, uint32_t row);
void con_get_cursor(uint32_t *col, uint32_t *row);

/* Basit sayi yazma (debug icin) */
void con_print_hex(uint64_t val);        /* 0x... formatinda hex           */
void con_print_dec(uint64_t val);        /* decimal                        */
