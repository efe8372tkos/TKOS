#pragma once
#include "types.h"

/*
 * TKOS - Sistem Cagrilari (Syscall) Arayuzu
 *
 * TKOS Ring0-only (TempleOS tarzi, izolasyon yok) calistigi icin
 * syscall'lar bir GUVENLIK SINIRI DEGIL, sadece duzenli bir API
 * katmanidir: diskten yuklenen (TKX) uygulamalarin kernel
 * servislerine (konsol, bellek, zamanlayici) int 0x80 uzerinden
 * eristigi fonksiyonlar butunudur.
 *
 * Cagri kurali - isr_stubs.asm (isr_syscall) ile BIREBIR uyumlu
 * olmali:
 *   RAX = syscall numarasi (SYS_*)
 *   RDI = argument 1
 *   RSI = argument 2
 *   RDX = argument 3
 *   RCX = argument 4
 *   R8  = argument 5
 *   int 0x80
 *   Donus degeri -> RAX
 *
 * Bu numaralar kullanici-alani (TKX uygulama) tarafinda da
 * BILINMELI - bkz. tkx_abi.h (uygulama gelistirme icin paylasilan
 * sarmalayici header, kernel disinda).
 */

#define SYS_EXIT        0   /* a1=cikis kodu (int)                     */
#define SYS_WRITE       1   /* a1=const char* (NUL sonlu string)        */
#define SYS_PUTC        2   /* a1=karakter (char)                       */
#define SYS_SLEEP       3   /* a1=ms (pit tick, 1000Hz -> 1:1)          */
#define SYS_YIELD       4   /* arguman yok - CPU'yu gonullu birak       */
#define SYS_GETPID      5   /* arguman yok -> donus: gorev numarasi     */
#define SYS_ALLOC       6   /* a1=boyut(byte) -> donus: ptr (kmalloc)   */
#define SYS_FREE        7   /* a1=ptr (kfree)                          */
#define SYS_UPTIME_MS   8   /* arguman yok -> donus: sistem calisma ms  */

#define SYS_COUNT       9

/*
 * syscall_dispatch() - int 0x80 assembly stub'i (isr_syscall)
 * tarafindan cagrilir. Bilinmeyen syscall numarasi icin
 * (uint64_t)-1 doner.
 */
uint64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2,
                           uint64_t a3, uint64_t a4, uint64_t a5);
