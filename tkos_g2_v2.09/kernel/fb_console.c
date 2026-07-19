#include "fb_console.h"
#include "framebuffer.h"
#include "font.h"
#include "types.h"

#define FONT_W  8
#define FONT_H  8
#define COLS   (640 / FONT_W)   /* 80 */
#define ROWS   (480 / FONT_H)   /* 60 */

static uint32_t cur_col  = 0;
static uint32_t cur_row  = 0;
static uint8_t  color_fg = 0;
static uint8_t  color_bg = 15;

static void render_char(uint32_t col, uint32_t row, char c, uint8_t fg, uint8_t bg) {
    const uint8_t *glyph = font_get(c);
    uint32_t px = col * FONT_W;
    uint32_t py = row * FONT_H;
    for (uint32_t y = 0; y < FONT_H; y++) {
        uint8_t bits = glyph[y];
        for (uint32_t x = 0; x < FONT_W; x++)
           fb_put_pixel(px + x, py + y, (bits & (1 << x)) ? fg : bg); 
    }
}

static void scroll_up(void) {
    cur_row = ROWS - 1;
    fb_fill_rect(0, cur_row * FONT_H, COLS * FONT_W, FONT_H, color_bg);
}

void con_init(uint8_t fg, uint8_t bg) {
    color_fg = fg; color_bg = bg; cur_col = 0; cur_row = 0;
}

void con_clear(void) {
    fb_fill(color_bg); cur_col = 0; cur_row = 0;
}

void con_set_color(uint8_t fg, uint8_t bg) { color_fg = fg; color_bg = bg; }
void con_set_cursor(uint32_t col, uint32_t row) {
    if (col < COLS) cur_col = col;
    if (row < ROWS) cur_row = row;
}
void con_get_cursor(uint32_t *col, uint32_t *row) {
    if (col) *col = cur_col;
    if (row) *row = cur_row;
}

void con_putchar(char c) {
    if (c == '\n') {
        for (uint32_t x = cur_col; x < COLS; x++)
            render_char(x, cur_row, ' ', color_fg, color_bg);
        cur_col = 0; cur_row++;
        if (cur_row >= ROWS) scroll_up();
        return;
    }
    if (c == '\r') { cur_col = 0; return; }
    if (c == '\b') {
        if (cur_col > 0) { cur_col--; render_char(cur_col, cur_row, ' ', color_fg, color_bg); }
        return;
    }
    render_char(cur_col, cur_row, c, color_fg, color_bg);
    cur_col++;
    if (cur_col >= COLS) { cur_col = 0; cur_row++; if (cur_row >= ROWS) scroll_up(); }
}

void con_print(const char *str) {
    if (!str) return;
    while (*str) con_putchar(*str++);
}

void con_print_at(uint32_t col, uint32_t row, const char *str, uint8_t fg, uint8_t bg) {
    if (!str || col >= COLS || row >= ROWS) return;
    uint32_t sc = cur_col, sr = cur_row;
    uint8_t  sf = color_fg, sb = color_bg;
    cur_col = col; cur_row = row; color_fg = fg; color_bg = bg;
    while (*str && cur_col < COLS) con_putchar(*str++);
    cur_col = sc; cur_row = sr; color_fg = sf; color_bg = sb;
}

void con_print_hex(uint64_t val) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[19];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++)
        buf[2 + i] = hex[(val >> (60 - i * 4)) & 0xF];
    buf[18] = '\0';
    con_print(buf);
}

void con_print_dec(uint64_t val) {
    char buf[21]; int i = 20; buf[i] = '\0';
    if (val == 0) { con_putchar('0'); return; }
    while (val > 0 && i > 0) { buf[--i] = '0' + (val % 10); val /= 10; }
    con_print(&buf[i]);
}
