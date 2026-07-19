#pragma once
#include "types.h"
















void con_init(uint8_t fg, uint8_t bg);   
void con_clear(void);                    
void con_putchar(char c);                
void con_print(const char *str);         
void con_print_at(uint32_t col, uint32_t row,
                  const char *str,
                  uint8_t fg, uint8_t bg); 
void con_set_color(uint8_t fg, uint8_t bg);
void con_set_cursor(uint32_t col, uint32_t row);
void con_get_cursor(uint32_t *col, uint32_t *row);


void con_print_hex(uint64_t val);        
void con_print_dec(uint64_t val);        
