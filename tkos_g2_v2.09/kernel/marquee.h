#pragma once
#include "types.h"

/*
 * TKOS - Piksel Bazli Kayan Yazi (Marquee)
 *
 * con_print_at() sadece 8px'lik font gridine gore calisir,
 * bu yuzden gercek puruzsuz kayma icin font_get()'i dogrudan
 * kullanip her tick'te 1 piksel oteleyerek yeniden ciziyoruz.
 *
 * Klasik ticker davranisi: metin sagdan girer, soldan cikar,
 * tamamen ciktiktan sonra basa doner (surekli dongu).
 */

#define MARQUEE_TEXT_MAX   64
#define MARQUEE_MAX_COUNT  4

typedef struct {
    int32_t x, y, w, h;         /* kayan yazi kutusu (h genelde FONT_H=8) */
    char    text[MARQUEE_TEXT_MAX];
    uint8_t fg, bg;
    int32_t offset;             /* mevcut kayma miktari (piksel) */
    int32_t text_px_w;          /* metnin toplam piksel genisligi */
    int     tick_div;           /* her kac tick'te 1 piksel kaysin (hiz) */
    int     tick_count;
    int     active;
} marquee_t;

/*
 * marquee_create() - yeni bir kayan yazi olusturur ve hemen ciziyor.
 * speed_div: 1 = her tick'te kayar (hizli), 4 = her 4 tick'te bir (yavas).
 * Basari: 0..MARQUEE_MAX_COUNT-1 arasi id doner, slot doluysa -1.
 */
int  marquee_create(int32_t x, int32_t y, int32_t w, int32_t h,
                     const char *text, uint8_t fg, uint8_t bg, int speed_div);

void marquee_destroy(int id);

/*
 * marquee_tick() - her PIT tick'inde (ya da idle loop'ta) cagir.
 * Aktif tum kayan yazilarin offsetini ilerletip yeniden cizer.
 */
void marquee_tick(void);
