#include "string.h"
#include "types.h"

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return pa[i] - pb[i];
    }
    return 0;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (uint8_t)a[i] - (uint8_t)b[i];
        if (!a[i]) break;
    }
    return 0;
}

char *strcat(char *dst, const char *src) {
    char *d = dst + strlen(dst);
    while ((*d++ = *src++));
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return NULL;
}

void utoa(uint64_t val, char *buf, int base) {
    static const char digits[] = "0123456789ABCDEF";
    char tmp[65];
    int  i = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    while (val > 0) {
        tmp[i++] = digits[val % base];
        val /= base;
    }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

void itoa(int64_t val, char *buf, int base) {
    if (val < 0 && base == 10) {
        buf[0] = '-';
        utoa((uint64_t)(-val), buf + 1, base);
    } else {
        utoa((uint64_t)val, buf, base);
    }
}

/*
 * str_to_dec() - Ondalik (decimal) string'i uint64_t'ye cevirir.
 * Basinda/sonunda bosluklari tolere eder; ilk gecersiz karakterde
 * durur. Hic rakam okunamadiysa veya sayidan sonra bosluk disinda
 * bir karakter kaldiysa *ok = 0 olur.
 */
uint64_t str_to_dec(const char *s, int *ok) {
    if (ok) *ok = 0;
    if (!s) return 0;

    while (*s == ' ') s++;

    uint64_t val = 0;
    int any = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (uint64_t)(*s - '0');
        s++;
        any = 1;
    }

    while (*s == ' ') s++;

    if (any && *s == '\0') {
        if (ok) *ok = 1;
    }
    return val;
}

/*
 * str_to_hex() - Onaltilik (hex) string'i uint64_t'ye cevirir.
 * Basinda opsiyonel "0x"/"0X" onekini kabul eder. Kurallar
 * str_to_dec() ile aynidir.
 */
uint64_t str_to_hex(const char *s, int *ok) {
    if (ok) *ok = 0;
    if (!s) return 0;

    while (*s == ' ') s++;

    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;

    uint64_t val = 0;
    int any = 0;
    while (*s) {
        char c = *s;
        uint8_t d;
        if      (c >= '0' && c <= '9') d = (uint8_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint8_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (uint8_t)(c - 'A' + 10);
        else break;
        val = (val << 4) | d;
        s++;
        any = 1;
    }

    while (*s == ' ') s++;

    if (any && *s == '\0') {
        if (ok) *ok = 1;
    }
    return val;
}
