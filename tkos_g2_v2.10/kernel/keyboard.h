#pragma once
#include "types.h"

typedef void (*key_callback_t)(char c);

void keyboard_set_callback(key_callback_t cb);
void keyboard_handler();
