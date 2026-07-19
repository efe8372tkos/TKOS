[BITS 64]

;
; TKOS - ISR / IRQ Assembly Stub'lari
;
; TempleOS kints.HC INT_FAULT ve IRQ_TIMER mantigindan uyarlanmistir.
;
; Her interrupt icin iki katmanli yapilanma:
;   1. Assembly stub : register'lari kaydeder, C dispatch'i cagirir
;   2. C idt_dispatch(): uygun handler'i cagirir, EOI gonderir
;
; TempleOS TASK_CONTEXT_SAVE karsiligi: register push/pop blogu.
; Simdilik scheduler yok, sadece temel context kaydi.
;
; NOT: Bazi exception'lar CPU tarafindan otomatik error code iter
; (8, 10-14, 17, 21). Diger exception'lar icin dummy 0 iteriz
; ki stack yapisi her zaman ayni olsun.
;

; C dispatch fonksiyonu (idt.c)
extern idt_dispatch

; ------------------------------------------------
; Makrolar
; ------------------------------------------------

; Error code ITMEYEN exception icin stub
; (dummy 0 iterek stack'i esitliyoruz)
%macro ISR_NOERR 1
global isr%1
isr%1:
    cli
    push    qword 0         ; dummy error code
    push    qword %1        ; vektor numarasi
    jmp     isr_common
%endmacro

; Error code ITEN exception icin stub
; (CPU zaten itmis, sadece numara ekle)
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    push    qword %1        ; vektor numarasi (error code zaten stack'te)
    jmp     isr_common
%endmacro

; IRQ stub'i
%macro IRQ_STUB 1
global irq%1
irq%1:
    cli
    push    qword 0         ; dummy error code
    push    qword (0x20 + %1) ; vektor numarasi
    jmp     isr_common
%endmacro

; ------------------------------------------------
; Exception stub'lari
; TempleOS IntFaultHndlrsNew() runtime kod uretiminin
; derleme-zamani karsiligi.
; ------------------------------------------------
ISR_NOERR 0     ; I_DIV_ZERO
ISR_NOERR 1     ; I_SINGLE_STEP
ISR_NOERR 2     ; I_NMI
ISR_NOERR 3     ; I_BPT
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound Range
ISR_NOERR 6     ; Invalid Opcode
ISR_NOERR 7     ; Device Not Available
ISR_ERR   8     ; Double Fault      (error code var)
ISR_NOERR 9     ; Rezerv
ISR_ERR   10    ; Invalid TSS       (error code var)
ISR_ERR   11    ; Segment Not Pres  (error code var)
ISR_ERR   12    ; Stack Fault       (error code var)
ISR_ERR   13    ; GPF               (error code var)
ISR_ERR   14    ; Page Fault        (error code var)
ISR_NOERR 15    ; Rezerv
ISR_NOERR 16    ; x87 FP Exception
ISR_ERR   17    ; Alignment Check   (error code var)
ISR_NOERR 18    ; Machine Check
ISR_NOERR 19    ; SIMD FP Exception

; ------------------------------------------------
; IRQ stub'lari (PIC offset 0x20)
; TempleOS IRQ_TIMER, I_KEYBOARD karsiligi
; ------------------------------------------------
IRQ_STUB  0     ; IRQ0 -> Timer
IRQ_STUB  1     ; IRQ1 -> Keyboard
IRQ_STUB  2     ; IRQ2 -> Cascade
IRQ_STUB  3     ; IRQ3
IRQ_STUB  4     ; IRQ4
IRQ_STUB  5     ; IRQ5
IRQ_STUB  6     ; IRQ6
IRQ_STUB  7     ; IRQ7
IRQ_STUB  8     ; IRQ8  -> RTC
IRQ_STUB  9     ; IRQ9
IRQ_STUB  10    ; IRQ10
IRQ_STUB  11    ; IRQ11
IRQ_STUB  12    ; IRQ12 -> Mouse
IRQ_STUB  13    ; IRQ13
IRQ_STUB  14    ; IRQ14 -> ATA1
IRQ_STUB  15    ; IRQ15 -> ATA2

; ------------------------------------------------
; Bilinmeyen IRQ icin NOP handler
; TempleOS IntNop() karsiligi:
;   OutU8(0xA0, 0x20)  slave EOI
;   OutU8(0x20, 0x20)  master EOI
; ------------------------------------------------
global irq_nop
irq_nop:
    cli
    push    qword 0
    push    qword 0xFF      ; bilinmeyen vektor
    jmp     isr_common

; ------------------------------------------------
; Ortak ISR giris noktasi
; TempleOS TASK_CONTEXT_SAVE karsiligi.
;
; Stack durumu bu noktada (asagidan yukari):
;   [RSP+0]  vektor numarasi   (bizim push'umuz)
;   [RSP+8]  error code        (bizim veya CPU'nun push'u)
;   [RSP+16] RIP               (CPU'nun push'u)
;   [RSP+24] CS                (CPU'nun push'u)
;   [RSP+32] RFLAGS            (CPU'nun push'u)
;   [RSP+40] RSP               (CPU'nun push'u - privilege degisiminde)
;   [RSP+48] SS                (CPU'nun push'u - privilege degisiminde)
; ------------------------------------------------
isr_common:
    ; Tum genel amacli register'lari kaydet
    ; TempleOS TASK_CONTEXT_SAVE ile ayni siralama
    push    rax
    push    rbx
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    rbp
    push    r8
    push    r9
    push    r10
    push    r11
    push    r12
    push    r13
    push    r14
    push    r15

    ; Data segment'leri kernel segment'e ayarla
    mov     ax, 0x10        ; DATA_SEG (stage2.asm ile uyumlu)
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax

    ; idt_dispatch(vektor_num, *frame) cagir
    ;
    ; Stack'teki konum (15 register push sonrasi):
    ;   [RSP + 15*8 + 0]   = vektor numarasi
    ;   [RSP + 15*8 + 8]   = error code
    ;   [RSP + 15*8 + 16]  = int_frame_t baslangici (RIP)
    ;
    ; rdi = birinci arguman: vektor numarasi
    ; rsi = ikinci arguman:  int_frame_t pointer'i
    ;
    mov     rdi, [rsp + 15*8]       ; vektor numarasi
    lea     rsi, [rsp + 15*8 + 16]  ; int_frame_t* (RIP'ten baslayan)
    call    idt_dispatch

    ; Register'lari geri yukle
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rbp
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    ; Vektor numarasi ve error code'u temizle
    add     rsp, 16

    iretq

   ; ------------------------------------------------
; int 0x80 - Sistem Cagrisi (Syscall) Giris Noktasi
;
; NOT: Bu, genel isr_common yolunu KULLANMAZ - idt_dispatch'in
; int_frame_t'si genel amacli register'lari icermiyor, syscall
; argumanlarina erismek icin dogrudan bu ozel stub gerekli.
;
; Cagri kurali (syscall.h ile birebir uyumlu):
;   RAX=no, RDI=a1, RSI=a2, RDX=a3, RCX=a4, R8=a5
;   Donus degeri RAX'ta.
;
; RBX/RBP/R12-R15 (callee-saved) syscall_dispatch() tarafindan
; C ABI geregi otomatik korunur; ayrica saklamaya gerek yok.
; ------------------------------------------------
extern syscall_dispatch

global isr_syscall
isr_syscall:
    ; Argumanlari C cagri sirasina donustur: push/pop ile
    ; cakisan register'lari bozmadan yeniden siralama.
    push    r8              ; a5
    push    rcx              ; a4
    push    rdx              ; a3
    push    rsi              ; a2
    push    rdi              ; a1
    push    rax              ; syscall no

    pop     rdi              ; rdi = syscall no
    pop     rsi              ; rsi = a1
    pop     rdx              ; rdx = a2
    pop     rcx              ; rcx = a3
    pop     r8               ; r8  = a4
    pop     r9               ; r9  = a5

    call    syscall_dispatch
    ; syscall_dispatch donus degeri zaten RAX'ta -> iretq sonrasi
    ; cagiran kodun RAX'inda gorunecek.

    iretq 
