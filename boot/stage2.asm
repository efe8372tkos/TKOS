[BITS 16]
[ORG 0x7E00]

stage2_start:
    ; Segmentleri ve Stack'i güvenliğe alalım
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov si, msg_stage2
    call print_string_16

; ------------------------------------------------
; VBE SET MODE (640x480x8bpp)
; ------------------------------------------------
vbe_init:
    mov ax, 0x4F01
    mov cx, 0x0101
    mov di, 0x8000
    int 0x10
    cmp ax, 0x004F
    jne vbe_failed

    mov ax, 0x4F02
    mov bx, 0x0101
    or  bx, 0x4000    ; Linear Framebuffer (LFB) bitini set et
    int 0x10
    cmp ax, 0x004F
    jne vbe_failed

    ; Framebuffer bilgi alanını temizle
    xor ax, ax
    mov es, ax
    mov di, 0x9000
    mov cx, 16
    rep stosw

    ; Bilgileri Kernel için kaydet (0x9000 bölgesine)
    ; PhysBasePtr (32-bit fiziksel adres)
    mov eax, [0x8000 + 40]
    mov [0x9000], eax
    
    ; Pitch (Satır başı byte sayısı)
    mov ax, [0x8000 + 16]
    mov [0x9004], ax
    
    ; BPP (Piksel başı bit)
    mov al, [0x8000 + 25]
    mov [0x9006], al

    ; DİKKAT: VBE moduna geçtikten sonra BIOS Print (int 10h ah=0E) KULLANILAMAZ!
    ; Bu yüzden ekrana "VBE OK" yazdırmıyoruz, direkt A20'ye geçiyoruz.
    jmp enable_a20

vbe_failed:
    ; Eğer VBE başarısız olursa standart metin modunda olduğumuz için yazdırabiliriz
    mov si, msg_vbe_fail
    call print_string_16
    jmp $ ; VBE yoksa sistemi durdur

; ------------------------------------------------
; A20 Etkinleştirme
; ------------------------------------------------
enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al

; ------------------------------------------------
; GDT Girişi (32-bit Protected Mode'a geçiş)
; ------------------------------------------------
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEG:protected_mode

; ------------------------------------------------
; PRINT 16-bit (Sadece Metin Modunda Çalışır)
; ------------------------------------------------
print_string_16:
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

msg_stage2   db "Stage2 OK", 13, 10, 0
msg_vbe_fail db "VBE FAIL! OS Halted.", 13, 10, 0

; ------------------------------------------------
; GDT Yapısı
; ------------------------------------------------
align 8
gdt_start:
gdt_null: dq 0
gdt_code:
    dw 0xFFFF, 0
    db 0, 10011010b, 11001111b, 0
gdt_data:
    dw 0xFFFF, 0
    db 0, 10010010b, 11001111b, 0
gdt_code_64: 
    dw 0, 0
    db 0, 10011010b, 00100000b, 0
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG   equ gdt_code - gdt_start
DATA_SEG   equ gdt_data - gdt_start
CODE64_SEG equ gdt_code_64 - gdt_start

; ------------------------------------------------
; 32-bit Korumalı Mod (Paging Hazırlığı)
; ------------------------------------------------
[BITS 32]
protected_mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    ; --- Sayfa Tablolarını Dinamik Olarak Oluşturma (İlk 4GB'ı haritala) ---
    ; 0x10000 - 0x15FFF arasını sıfırla (6 Sayfa: 1 PML4, 1 PDPT, 4 PD)
    mov edi, 0x10000
    xor eax, eax
    mov ecx, 6144  ; 6 * 1024 dword (24KB)
    rep stosd

    ; PML4 (0x10000) -> PDPT (0x11000)
    mov dword [0x10000], 0x11003

    ; PDPT (0x101000) -> 4 adet PD (0x102000, 0x103000, 0x104000, 0x105000)
    mov dword [0x11000], 0x12003
    mov dword [0x11008], 0x13003
    mov dword [0x11010], 0x14003
    mov dword [0x11018], 0x15003

    ; PD'leri doldur (2048 giriş x 2MB = 4GB bellek haritası)
    mov edi, 0x12000
    mov eax, 0x00000083 ; Present, R/W, Huge Page (2MB boyutu)
    mov ecx, 2048
.build_pd:
    mov [edi], eax
    add eax, 0x200000   ; Fiziksel adresi 2MB arttır
    add edi, 8          ; Tablodaki bir sonraki girişe geç
    loop .build_pd

    ; --- PAE ve Sayfalama (Paging) Aktifleştirme ---
    mov eax, 0x10000
    mov cr3, eax

    mov eax, cr4
    or eax, (1 << 5)        ; PAE aktif
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)        ; LME (Long Mode Enable) aktif
    wrmsr

    mov eax, cr0
    or eax, (1 << 31)       ; Paging aktif -> Uyumluluk Modundayız
    mov cr0, eax

    ; 64-bit Uzun Moda Güvenli Geçiş (Far Jump)
    jmp CODE64_SEG:long_mode

; 64-bit Uzun Mod
; ------------------------------------------------
[BITS 64]
long_mode:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

; ------------------------------------------------
; ATA PIO: sektor 7'den 1000 sektor -> 0x200000
; (üst sinir: 2041 sektor - sektor 2048'de FAT16 bölümü basliyor,
;  mkfs.fat --offset=2048 ile. Bunu asma!)
; ------------------------------------------------
    cld
    mov rdi, 0x200000
    mov rbx, 7
    mov r15, 1000

.sector_loop:
    mov rdx, 0x1F6
    mov rax, rbx
    shr rax, 24
    and rax, 0x0F
    or  rax, 0xE0
    out dx, al

    mov rdx, 0x1F2
    mov al, 1
    out dx, al

    mov rdx, 0x1F3
    mov rax, rbx
    out dx, al

    mov rdx, 0x1F4
    mov rax, rbx
    shr rax, 8
    out dx, al

    mov rdx, 0x1F5
    mov rax, rbx
    shr rax, 16
    out dx, al

    mov rdx, 0x1F7
    mov al, 0x20
    out dx, al

.wait_bsy:
    in  al, dx
    test al, 0x80
    jnz .wait_bsy

.wait_drq:
    in  al, dx
    test al, 0x08
    jz  .wait_drq

    mov rdx, 0x1F0
    mov rcx, 256
    rep insw

    inc rbx
    dec r15
    jnz .sector_loop

    mov rax, 0x200000
    jmp rax
