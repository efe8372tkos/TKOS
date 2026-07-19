#pragma once
#include "types.h"














#define BUTTON_LABEL_MAX 16
#define BUTTON_MAX_COUNT 16

typedef void (*button_callback_t)(void);

typedef struct {
    int32_t x, y, w, h;
    char label[BUTTON_LABEL_MAX];
    button_callback_t on_click;
    int pressed;   
    int active;    
} button_t;








int button_create(int32_t x, int32_t y, int32_t w, int32_t h,
                   const char *label, button_callback_t on_click);


void button_destroy(int id);






void button_handle_mouse(int32_t x, int32_t y, uint8_t buttons);


void button_draw(int id);
