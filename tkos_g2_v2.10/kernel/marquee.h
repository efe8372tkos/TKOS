#pragma once
#include "types.h"












#define MARQUEE_TEXT_MAX   64
#define MARQUEE_MAX_COUNT  4

typedef struct {
    int32_t x, y, w, h;         
    char    text[MARQUEE_TEXT_MAX];
    uint8_t fg, bg;
    int32_t offset;             
    int32_t text_px_w;          
    int     tick_div;           
    int     tick_count;
    int     active;
} marquee_t;






int  marquee_create(int32_t x, int32_t y, int32_t w, int32_t h,
                     const char *text, uint8_t fg, uint8_t bg, int speed_div);

void marquee_destroy(int id);






void marquee_tick(void);







void marquee_pause(void);
void marquee_resume(void);
