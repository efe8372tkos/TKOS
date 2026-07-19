#pragma once
#include "types.h"

/*
 * TKOS - Basit Buton Widget Sistemi
 *
 * mouse.c uzerine kurulu, keyboard_set_callback/mouse_set_callback
 * ile ayni "olay olunca cagir" desenini kullanir.
 *
 * Standart 3D buton davranisi:
 *   - Basmadan once: kabarik (raised) gorunum
 *   - Basiliyken:    basik (sunken) gorunum
 *   - Birakildiginda: hala ayni butonun uzerindeyse tiklama sayilir,
 *     baska yerde birakilirsa (surukleyip vazgecme) saymaz
 */

#define BUTTON_LABEL_MAX 16
#define BUTTON_MAX_COUNT 16

typedef void (*button_callback_t)(void);

typedef struct {
    int32_t x, y, w, h;
    char label[BUTTON_LABEL_MAX];
    button_callback_t on_click;
    int pressed;   /* su an mouse bu butonun uzerinde asagi tutuluyor mu */
    int active;    /* bu slot kullanimda mi */
} button_t;

/*
 * button_create() - yeni bir buton olusturur ve hemen ekrana ciziyor.
 * Basari: 0..BUTTON_MAX_COUNT-1 arasi id doner. Slot doluysa -1.
 *
 * Not: label 8x8 font gridine gore col/row olarak yazilir, x/y'nin
 * 8'in kati olmasi en temiz gorunumu verir (sart degil).
 */
int button_create(int32_t x, int32_t y, int32_t w, int32_t h,
                   const char *label, button_callback_t on_click);

/* button_destroy() - butonu kayitlardan cikarir (ekrandan silmez). */
void button_destroy(int id);

/*
 * button_handle_mouse() - mouse_set_callback() ile kaydedilecek
 * fonksiyon. mouse_callback_t ile birebir ayni imza.
 * Basma/birakma/tiklama tum mantigi burada.
 */
void button_handle_mouse(int32_t x, int32_t y, uint8_t buttons);

/* button_draw() - butonu (yeniden) ciz - normal veya basili gorunumde */
void button_draw(int id);
