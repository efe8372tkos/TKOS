#pragma once
#include "types.h"


void  *memcpy(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
void  *memmove(void *dst, const void *src, size_t n);


size_t strlen(const char *s);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcat(char *dst, const char *src);
char  *strchr(const char *s, int c);


void   itoa(int64_t val,  char *buf, int base);
void   utoa(uint64_t val, char *buf, int base);







uint64_t str_to_dec(const char *s, int *ok);   
uint64_t str_to_hex(const char *s, int *ok);   
