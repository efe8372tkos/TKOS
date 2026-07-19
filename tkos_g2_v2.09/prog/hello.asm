[BITS 64]
default rel                 ; TUM adresleme RIP-relative - PIC zorunlulugu

; --- TKX Basligi (32 byte, exec.h/tkx_header_t ile birebir ayni) ---
    dd 0x31584B54            ; magic "TKX1"
    dd 1                     ; version
    dq entry - image_start   ; entry_offset
    dq image_end - image_start ; image_size
    dq 8192                  ; stack_size (8KB)

image_start:
entry:
    lea rdi, [msg]           ; SYS_WRITE(msg) - RIP-relative, PIC-guvenli
    mov rax, 1
    int 0x80

    xor rdi, rdi              ; SYS_EXIT(0)
    mov rax, 0
    int 0x80

.hang:                        ; guvenlik agi
    jmp .hang

msg: db "Merhaba TKOS'tan!", 10, 0

image_end:
