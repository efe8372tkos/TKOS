#include "button.h"
#include "framebuffer.h"
#include "fb_console.h"
#include "mouse.h"
#include "types.h"







#define COL_FACE  7   
#define COL_LIGHT 15  
#define COL_DARK  8   
#define COL_TEXT  0   

#define FONT_W 8
#define FONT_H 8

static button_t buttons[BUTTON_MAX_COUNT];
static int      pressed_id = -1;  
static uint8_t  prev_mouse_buttons = 0;

static int point_in_button(int32_t x, int32_t y, const button_t *b) {
    return x >= b->x && x < b->x + b->w &&
           y >= b->y && y < b->y + b->h;
}


static void draw_bevel(const button_t *b, int is_pressed) {
    fb_fill_rect((uint32_t)b->x, (uint32_t)b->y, (uint32_t)b->w, (uint32_t)b->h, COL_FACE);

    uint8_t top_left  = is_pressed ? COL_DARK  : COL_LIGHT;
    uint8_t bot_right = is_pressed ? COL_LIGHT : COL_DARK;

    fb_fill_rect((uint32_t)b->x, (uint32_t)b->y, (uint32_t)b->w, 1, top_left);              
    fb_fill_rect((uint32_t)b->x, (uint32_t)b->y, 1, (uint32_t)b->h, top_left);              
    fb_fill_rect((uint32_t)b->x, (uint32_t)(b->y + b->h - 1), (uint32_t)b->w, 1, bot_right); 
    fb_fill_rect((uint32_t)(b->x + b->w - 1), (uint32_t)b->y, 1, (uint32_t)b->h, bot_right); 
}

void button_draw(int id) {
    if (id < 0 || id >= BUTTON_MAX_COUNT || !buttons[id].active) return;
    button_t *b = &buttons[id];

    draw_bevel(b, b->pressed);

    uint32_t col = (uint32_t)(b->x / FONT_W);
    uint32_t row = (uint32_t)(b->y / FONT_H);
    con_print_at(col, row, b->label, COL_TEXT, COL_FACE);
}

int button_create(int32_t x, int32_t y, int32_t w, int32_t h,
                   const char *label, button_callback_t on_click) {
    for (int i = 0; i < BUTTON_MAX_COUNT; i++) {
        if (buttons[i].active) continue;

        buttons[i].x = x;
        buttons[i].y = y;
        buttons[i].w = w;
        buttons[i].h = h;
        buttons[i].on_click = on_click;
        buttons[i].pressed = 0;
        buttons[i].active = 1;

        int j = 0;
        while (label && label[j] && j < BUTTON_LABEL_MAX - 1) {
            buttons[i].label[j] = label[j];
            j++;
        }
        buttons[i].label[j] = '\0';

        button_draw(i);
        return i;
    }
    return -1; 
}

void button_destroy(int id) {
    if (id < 0 || id >= BUTTON_MAX_COUNT) return;
    buttons[id].active = 0;
    if (pressed_id == id) pressed_id = -1;
}

void button_handle_mouse(int32_t x, int32_t y, uint8_t buttons_state) {
    int left_now  = (buttons_state       & MOUSE_BTN_LEFT) != 0;
    int left_prev = (prev_mouse_buttons  & MOUSE_BTN_LEFT) != 0;

    if (left_now && !left_prev) {
        
        for (int i = 0; i < BUTTON_MAX_COUNT; i++) {
            if (buttons[i].active && point_in_button(x, y, &buttons[i])) {
                pressed_id = i;
                buttons[i].pressed = 1;
                button_draw(i);
                break;
            }
        }
    } else if (!left_now && left_prev) {
        


        if (pressed_id >= 0 && buttons[pressed_id].active) {
            button_t *b = &buttons[pressed_id];
            b->pressed = 0;
            button_draw(pressed_id);
            if (point_in_button(x, y, b) && b->on_click) {
                b->on_click();
            }
        }
        pressed_id = -1;
    }

    prev_mouse_buttons = buttons_state;
}
