#pragma once
#include "types.h"
#include "idt.h"









#define MOUSE_BTN_LEFT   0x01
#define MOUSE_BTN_RIGHT  0x02
#define MOUSE_BTN_MIDDLE 0x04

typedef void (*mouse_callback_t)(int32_t x, int32_t y, uint8_t buttons);










void mouse_init(void);







void mouse_handler(int_frame_t *frame);


void mouse_set_callback(mouse_callback_t cb);

int32_t mouse_get_x(void);
int32_t mouse_get_y(void);
uint8_t mouse_get_buttons(void);


void mouse_show_cursor(void);
void mouse_hide_cursor(void);
