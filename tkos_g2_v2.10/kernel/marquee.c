#include "marquee.h"
#include "framebuffer.h"
#include "font.h"
#include "types.h"








#define FONT_W 8
#define FONT_H 8

static marquee_t marquees[MARQUEE_MAX_COUNT];
static int g_marquee_paused = 0;




static void clipped_put_pixel(const marquee_t *m, int32_t x, int32_t y, uint8_t color) {
    if (x < m->x || x >= m->x + m->w) return;
    if (y < m->y || y >= m->y + m->h) return;
    fb_put_pixel((uint32_t)x, (uint32_t)y, color);
}

static void draw_char_at(const marquee_t *m, int32_t px, int32_t py, char c) {
    const uint8_t *glyph = font_get(c);
    for (int32_t row = 0; row < FONT_H; row++) {
        uint8_t bits = glyph[row];
        for (int32_t col = 0; col < FONT_W; col++) {
            uint8_t color = (bits & (1 << col)) ? m->fg : m->bg;
            clipped_put_pixel(m, px + col, py + row, color);
        }
    }
}

static void marquee_draw(marquee_t *m) {
    
    fb_fill_rect((uint32_t)m->x, (uint32_t)m->y, (uint32_t)m->w, (uint32_t)m->h, m->bg);

    


    int32_t start_x = m->x + m->w - m->offset;

    int32_t len = 0;
    while (m->text[len]) len++;

    for (int32_t i = 0; i < len; i++) {
        int32_t cx = start_x + i * FONT_W;
        if (cx + FONT_W <= m->x || cx >= m->x + m->w) continue; 
        draw_char_at(m, cx, m->y, m->text[i]);
    }
}

int marquee_create(int32_t x, int32_t y, int32_t w, int32_t h,
                    const char *text, uint8_t fg, uint8_t bg, int speed_div) {
    for (int i = 0; i < MARQUEE_MAX_COUNT; i++) {
        if (marquees[i].active) continue;

        marquee_t *m = &marquees[i];
        m->x = x; m->y = y; m->w = w; m->h = h;
        m->fg = fg; m->bg = bg;
        m->offset = 0;
        m->tick_div = (speed_div > 0) ? speed_div : 1;
        m->tick_count = 0;
        m->active = 1;

        int j = 0;
        while (text && text[j] && j < MARQUEE_TEXT_MAX - 1) {
            m->text[j] = text[j];
            j++;
        }
        m->text[j] = '\0';
        m->text_px_w = j * FONT_W;

        marquee_draw(m);
        return i;
    }
    return -1; 
}

void marquee_destroy(int id) {
    if (id < 0 || id >= MARQUEE_MAX_COUNT) return;
    marquees[id].active = 0;
}

void marquee_tick(void) {
    if (g_marquee_paused) return;

    for (int i = 0; i < MARQUEE_MAX_COUNT; i++) {
        marquee_t *m = &marquees[i];
        if (!m->active) continue;

        m->tick_count++;
        if (m->tick_count < m->tick_div) continue;
        m->tick_count = 0;

        m->offset++;
        

        if (m->offset > m->text_px_w + m->w) {
            m->offset = 0;
        }

        marquee_draw(m);
    }
}

void marquee_pause(void)  { g_marquee_paused = 1; }
void marquee_resume(void) { g_marquee_paused = 0; }
