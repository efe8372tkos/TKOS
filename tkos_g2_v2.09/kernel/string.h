#pragma once
#include "types.h"

/* Bellek */
void  *memcpy(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
void  *memmove(void *dst, const void *src, size_t n);

/* String */
size_t strlen(const char *s);
char  *strcpy(char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strcat(char *dst, const char *src);
char  *strchr(const char *s, int c);

/* Sayi <-> String */
void   itoa(int64_t val,  char *buf, int base);
void   utoa(uint64_t val, char *buf, int base);

/*
 * String -> Sayi ayristiricilar (shell komut argumanlari icin).
 * @ok : basarili/basarisiz bilgisini yazar (NULL verilirse yoksayilir).
 *       Basarisiz durumlar: bos string, gecersiz karakter, veya
 *       sayidan sonra bosluk disinda artik karakter kalmasi.
 */
uint64_t str_to_dec(const char *s, int *ok);   /* "123"        -> 123 */
uint64_t str_to_hex(const char *s, int *ok);   /* "0x1A" / "1A" -> 26 */
