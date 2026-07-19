#include "mouse.h"
#include "framebuffer.h"
#include "types.h"

/*
 * TKOS - PS/2 Fare Suruculu
 * keyboard.c / pic.c ile ayni port I/O deseni kullanilir.
 */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

#define PS2_STATUS_OUT_FULL 0x01  /* okunacak veri hazir */
#define PS2_STATUS_IN_FULL  0x02  /* yazma icin hala mesgul */

/* ------------------------------------------------
 * Dusuk seviye PS/2 controller I/O
 * ------------------------------------------------ */
static void ps2_wait_input(void) {
    int timeout = 100000;
    while (timeout-- && (inb(PS2_STATUS) & PS2_STATUS_IN_FULL)) { }
}

static void ps2_wait_output(void) {
    int timeout = 100000;
    while (timeout-- && !(inb(PS2_STATUS) & PS2_STATUS_OUT_FULL)) { }
}

/* Fareye (aux aygit) bir komut byte'i yaz */
static void mouse_write(uint8_t data) {
    ps2_wait_input();
    outb(PS2_CMD, 0xD4);      /* "sonraki byte aux aygita gidecek" */
    ps2_wait_input();
    outb(PS2_DATA, data);
}

/* Data portundan bir byte oku (hem aux veri hem controller cevaplari icin) */
static uint8_t ps2_read_data(void) {
    ps2_wait_output();
    return inb(PS2_DATA);
}

/* ------------------------------------------------
 * Paket durum makinesi
 * PS/2 fare paketi 3 byte'dir:
 *   byte0: bit0=SolBtn bit1=SagBtn bit2=OrtaBtn bit3=1(sabit)
 *          bit4=X isaret  bit5=Y isaret  bit6=X tasma bit7=Y tasma
 *   byte1: X hareketi (isaretli, dusuk 8 bit)
 *   byte2: Y hareketi (isaretli, dusuk 8 bit)
 * ------------------------------------------------ */
static uint8_t packet[3];
static uint8_t packet_index = 0;

static int32_t mouse_x = 320;
static int32_t mouse_y = 240;
static uint8_t mouse_buttons = 0;

static mouse_callback_t active_callback = 0;

/* ------------------------------------------------
 * Ok imleci - 8x8, basit iki renkli sekil
 * 0 = saydam (dokunma), 1 = siyah dis hat, 2 = beyaz ic dolgu
 * ------------------------------------------------ */
#define CURSOR_W 8
#define CURSOR_H 8

static const uint8_t cursor_shape[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0},
    {1,2,2,1,0,0,0,0},
    {1,2,2,2,1,0,0,0},
    {1,2,2,2,2,1,0,0},
    {1,2,2,1,1,0,0,0},
    {1,1,0,0,1,1,0,0},
};

static int      cursor_visible = 0;
static int      has_saved      = 0;
static int32_t  saved_x = 0, saved_y = 0;
static uint8_t  saved_bg[CURSOR_H][CURSOR_W];

static void cursor_restore(void) {
    if (!has_saved) return;
    for (int cy = 0; cy < CURSOR_H; cy++)
        for (int cx = 0; cx < CURSOR_W; cx++)
            fb_put_pixel(saved_x + cx, saved_y + cy, saved_bg[cy][cx]);
    has_saved = 0;
}

static void cursor_save_and_draw(int32_t x, int32_t y) {
    for (int cy = 0; cy < CURSOR_H; cy++)
        for (int cx = 0; cx < CURSOR_W; cx++)
            saved_bg[cy][cx] = fb_get_pixel(x + cx, y + cy);
    saved_x = x; saved_y = y; has_saved = 1;

    for (int cy = 0; cy < CURSOR_H; cy++) {
        for (int cx = 0; cx < CURSOR_W; cx++) {
            uint8_t px = cursor_shape[cy][cx];
            if (px == 1)      fb_put_pixel(x + cx, y + cy, 0);  /* siyah dis hat */
            else if (px == 2) fb_put_pixel(x + cx, y + cy, 15); /* beyaz ic dolgu */
            /* px == 0 -> dokunma, arka plan aynen kalsin */
        }
    }
}

static void mouse_redraw_cursor(void) {
    if (!cursor_visible || !fb_ready()) return;
    cursor_restore();
    cursor_save_and_draw(mouse_x, mouse_y);
}

void mouse_show_cursor(void) {
    cursor_visible = 1;
    mouse_redraw_cursor();
}

void mouse_hide_cursor(void) {
    cursor_restore();
    cursor_visible = 0;
}

void mouse_set_callback(mouse_callback_t cb) { active_callback = cb; }
int32_t mouse_get_x(void)       { return mouse_x; }
int32_t mouse_get_y(void)       { return mouse_y; }
uint8_t mouse_get_buttons(void) { return mouse_buttons; }

/* ------------------------------------------------
 * mouse_handler() - IRQ12 C handler
 * ------------------------------------------------ */
void mouse_handler(int_frame_t *frame) {
    (void)frame; /* kullanilmiyor, sadece int_handler_t imzasi icin */

    uint8_t data = inb(PS2_DATA);

    /* Senkronizasyon kontrolu: ilk byte'in bit3'u daima 1 olmali.
     * Degilse akis kaymis demektir, byte'i at ve yeniden hizala. */
    if (packet_index == 0 && !(data & 0x08)) {
        return;
    }

    packet[packet_index++] = data;
    if (packet_index < 3) return;
    packet_index = 0;

    uint8_t  flags = packet[0];
    int32_t  dx    = (int32_t)packet[1];
    int32_t  dy    = (int32_t)packet[2];

    /* Tasma bayraklari set ise paket guvenilmezdir, at */
    if (flags & 0xC0) return;

    if (flags & 0x10) dx |= (int32_t)0xFFFFFF00; /* X icin isaret genisletme */
    if (flags & 0x20) dy |= (int32_t)0xFFFFFF00; /* Y icin isaret genisletme */

    mouse_x += dx;
    mouse_y -= dy; /* PS/2: yukari pozitif, ekran: asagi pozitif */

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (fb_ready()) {
        int32_t max_x = (int32_t)fb_width()  - 1;
        int32_t max_y = (int32_t)fb_height() - 1;
        if (mouse_x > max_x) mouse_x = max_x;
        if (mouse_y > max_y) mouse_y = max_y;
    }

    mouse_buttons = flags & 0x07;

    mouse_redraw_cursor();

    if (active_callback) active_callback(mouse_x, mouse_y, mouse_buttons);
}

/* ------------------------------------------------
 * mouse_init() - PS/2 fareyi baslat
 * ------------------------------------------------ */
void mouse_init(void) {
    /* 1. Aux (fare) aygitini etkinlestir */
    ps2_wait_input();
    outb(PS2_CMD, 0xA8);

    /* 2. Controller yapilandirma byte'ini oku (0x20 komutu -> cevap 0x60'ta) */
    ps2_wait_input();
    outb(PS2_CMD, 0x20);
    uint8_t status = ps2_read_data();

    /* IRQ12'yi ac (bit1), fare saat hattini ac (bit5'i temizle) */
    status |= 0x02;
    status &= (uint8_t)~0x20;

    ps2_wait_input();
    outb(PS2_CMD, 0x60);
    ps2_wait_input();
    outb(PS2_DATA, status);

    /* 3. Fareyi varsayilan ayarlara sifirla, ACK bekle (0xFA) */
    mouse_write(0xF6);
    ps2_read_data();

    /* 4. Veri raporlamayi etkinlestir, ACK bekle */
    mouse_write(0xF4);
    ps2_read_data();

    packet_index  = 0;
    mouse_buttons = 0;
    mouse_x = fb_ready() ? (int32_t)fb_width()  / 2 : 320;
    mouse_y = fb_ready() ? (int32_t)fb_height() / 2 : 240;
}
