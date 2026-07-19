#pragma once
#include "types.h"
#include "idt.h"

/*
 * TKOS - PS/2 Fare Suruculu
 *
 * keyboard.c ile ayni desen: IRQ handler + callback sistemi.
 * IRQ12 -> irq12 (isr_stubs.asm, zaten mevcut) -> idt_dispatch -> mouse_handler()
 *
 * Buton bit maskeleri (PS/2 paket byte 0 ile ayni siralama)
 */
#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

typedef void (*mouse_callback_t)(int32_t x, int32_t y, uint8_t buttons);

/*
 * mouse_init() - PS/2 fareyi baslatir.
 *   1. Aux (fare) aygitini etkinlestir (0xA8)
 *   2. Controller config byte'inda IRQ12'yi ac
 *   3. Fareyi varsayilan ayarlara sifirla (0xF6)
 *   4. Veri raporlamayi etkinlestir (0xF4)
 * idt_set_handler(I_MOUSE, mouse_handler) ve
 * pic_unmask_irq(IRQ_MOUSE) BUNDAN SONRA cagirilmali.
 */
void mouse_init(void);

/*
 * mouse_handler() - IRQ12 C seviyesi handler.
 * int_handler_t (idt.h) ile ayni imza: void (*)(int_frame_t*).
 * frame kullanilmiyor, sadece idt_dispatch'in cagri seklini
 * karsilamak icin var.
 */
void mouse_handler(int_frame_t *frame);

/* Uygulama/shell tarafinin fare olaylarini dinlemesi icin */
void mouse_set_callback(mouse_callback_t cb);

int32_t mouse_get_x(void);
int32_t mouse_get_y(void);
uint8_t mouse_get_buttons(void);

/* Ekrandaki ok imlecini goster/gizle */
void mouse_show_cursor(void);
void mouse_hide_cursor(void);
