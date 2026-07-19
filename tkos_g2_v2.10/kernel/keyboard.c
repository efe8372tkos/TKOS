#include "keyboard.h"
#include "types.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define SC_ESCAPE    0x01
#define SC_BACKSPACE 0x0E
#define SC_ENTER     0x1C
#define SC_LSHIFT    0x2A
#define SC_RSHIFT    0x36

static int shift_held = 0;
static key_callback_t active_callback = 0;

static const char scancode_to_ascii[] = {
    0,   0, '1','2','3','4','5','6','7','8',
    '9','0','-','=', 0,   0, 'q','w','e','r',
    't', 'y','u','i','o','p','[',']', 0,   0,
    'a', 's','d','f','g','h','j','k','l',';',
    '\'','`', 0,'\\','z','x','c','v','b','n',
    'm', ',','.','/', 0,   0,   0, ' '
};

static const char scancode_to_ascii_shift[] = {
    0,   0, '!','@','#','$','%','^','&','*',
    '(', ')','_','+', 0,   0, 'Q','W','E','R',
    'T', 'Y','U','I','O','P','{','}', 0,   0,
    'A', 'S','D','F','G','H','J','K','L',':',
    '"', '~', 0,  '|','Z','X','C','V','B','N',
    'M', '<','>','?', 0,   0,   0, ' '
};

void keyboard_set_callback(key_callback_t cb) {
    active_callback = cb;
}

void keyboard_handler() {
    uint8_t scancode = inb(0x60);

    
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == SC_LSHIFT || released == SC_RSHIFT) {
            shift_held = 0;
        }
        return;
    }

    
    if (scancode == SC_LSHIFT || scancode == SC_RSHIFT) {
        shift_held = 1;
        return;
    }

    if (!active_callback) {
        return; 
    }

    


    if (scancode == SC_ESCAPE) {
        active_callback('\x1B');
        return;
    }

    if (scancode == SC_BACKSPACE) {
        active_callback('\b');
        return;
    }

    if (scancode == SC_ENTER) {
        active_callback('\n');
        return;
    }

    if (scancode < sizeof(scancode_to_ascii)) {
        char c = shift_held ? scancode_to_ascii_shift[scancode]
                             : scancode_to_ascii[scancode];
        if (c) {
            active_callback(c);
        }
    }
}
