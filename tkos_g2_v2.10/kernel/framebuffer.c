#include "framebuffer.h"
#include "types.h"

static uint8_t  *fb_addr      = (uint8_t*)0;
static uint16_t  fb_pitch_val = 0;
static uint32_t  fb_width_val = 0;
static uint32_t  fb_height_val= 0;
static uint8_t   fb_bpp_val   = 0;
static int       fb_initialized = 0;

int fb_init(void) {
    volatile uint8_t *info = (volatile uint8_t *)0x9000;

    uint32_t addr = (uint32_t)info[0] | ((uint32_t)info[1]<<8)
                  | ((uint32_t)info[2]<<16) | ((uint32_t)info[3]<<24);
    uint16_t pitch = (uint16_t)info[4] | ((uint16_t)info[5]<<8);
    uint8_t  bpp   = info[6];

    
    if (addr == 0) {
        addr  = 0xE0000000;
        pitch = 640;
        bpp   = 8;
    }

    if (pitch == 0) return 0;

    fb_addr      = (uint8_t *)(uintptr_t)addr;
    fb_pitch_val = pitch;
    fb_bpp_val   = bpp;

    if (bpp == 8) {
        fb_width_val  = pitch;
        fb_height_val = 480;
    } else if (bpp == 24) {
        fb_width_val  = pitch / 3;
        fb_height_val = 768;
    } else if (bpp == 32) {
        fb_width_val  = pitch / 4;
        fb_height_val = 480;
    } else {
        return 0;
    }

    fb_initialized = 1;
    return 1;
}

uint32_t fb_width(void)  { return fb_width_val;   }
uint32_t fb_height(void) { return fb_height_val;  }
uint8_t  fb_bpp(void)    { return fb_bpp_val;     }
int      fb_ready(void)  { return fb_initialized; }

void fb_put_pixel(uint32_t x, uint32_t y, uint8_t color) {
    if (!fb_initialized) return;
    if (x >= fb_width_val || y >= fb_height_val) return;
    fb_addr[y * fb_pitch_val + x] = color;
}

uint8_t fb_get_pixel(uint32_t x, uint32_t y) {
    if (!fb_initialized) return 0;
    if (x >= fb_width_val || y >= fb_height_val) return 0;
    return fb_addr[y * fb_pitch_val + x];
}

void fb_fill(uint8_t color) {
    if (!fb_initialized) return;
    for (uint32_t y = 0; y < fb_height_val; y++) {
        uint8_t *row = fb_addr + y * fb_pitch_val;
        for (uint32_t x = 0; x < fb_width_val; x++)
            row[x] = color;
    }
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t color) {
    if (!fb_initialized) return;
    uint32_t xe = x + w; if (xe > fb_width_val)  xe = fb_width_val;
    uint32_t ye = y + h; if (ye > fb_height_val) ye = fb_height_val;
    for (uint32_t row = y; row < ye; row++) {
        uint8_t *line = fb_addr + row * fb_pitch_val;
        for (uint32_t col = x; col < xe; col++)
            line[col] = color;
    }
}
