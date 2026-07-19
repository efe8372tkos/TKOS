[BITS 64]

global _start
extern kernel_main

_start:
    ; Segment registerlarini sifirla
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Stack: 0x290000
    mov rax, 0x290000
    mov rsp, rax
    xor rbp, rbp

    ; Direction flag temizle
    cld

    ; SSE aktifle
    mov rax, cr0
    and rax, ~(1 << 2)
    or  rax,  (1 << 1)
    mov cr0, rax
    mov rax, cr4
    or  rax, (1 << 9) | (1 << 10)
    mov cr4, rax

    call kernel_main

.hang:
    hlt
    jmp .hang
