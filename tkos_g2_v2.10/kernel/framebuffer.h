#pragma once
#include "types.h"









int  fb_init(void);                                        
void fb_put_pixel(uint32_t x, uint32_t y, uint8_t color); 
uint8_t fb_get_pixel(uint32_t x, uint32_t y);              
void fb_fill(uint8_t color);                               
void fb_fill_rect(uint32_t x, uint32_t y,
                  uint32_t w, uint32_t h, uint8_t color);


uint32_t fb_width(void);
uint32_t fb_height(void);
uint8_t  fb_bpp(void);
int      fb_ready(void);  
