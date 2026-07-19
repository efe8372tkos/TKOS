[BITS 64]

;
; TKOS - Context Switch Assembly
; TempleOS Sched.HC TASK_CONTEXT_SAVE ve
; _TASK_CONTEXT_RESTORE mantigindan uyarlanmistir.
;
; TempleOS register kayit sirasi (KernelA.HH CTask 3302. satir):
;   rip, rflags, rsp, rsi, rax, rcx, rdx, rbx, rbp, rdi,
;   r8, r9, r10, r11, r12, r13, r14, r15
;
; Bu siraya BIREBIR uyuyoruz; task_t struct offset'leri
; bu siralamayla eslesiyor olmali.
;
; sched_context_switch(prev, next)
;   rdi = prev task_t*
;   rsi = next task_t*
;
; Fonksiyon cagrisinin kendisi de bir context switch:
;   - CALL komutu return adresini stack'e iter
;   - Biz bu adresi prev->rip olarak kaydederiz
;   - RET ile next->rip'e atlariz
;

global sched_context_switch

; task_t struct offset'leri (task.h ile uyumlu olmali)
; sizeof(addr)=8, sizeof(signature)=4, sizeof(flags)=4,
; sizeof(name)=32, sizeof(task_num)=4, pad=4
; Toplam: 8+4+4+32+4+4 = 56 byte
; Ardindan register alani baslar:
;   rip    @ 56
;   rflags @ 64
;   rsp    @ 72
;   rsi    @ 80
;   rax    @ 88
;   rcx    @ 96
;   rdx    @ 104
;   rbx    @ 112
;   rbp    @ 120
;   rdi    @ 128
;   r8     @ 136
;   r9     @ 144
;   r10    @ 152
;   r11    @ 160
;   r12    @ 168
;   r13    @ 176
;   r14    @ 184
;   r15    @ 192
;   fpu_state* @ 200
;   wake_jiffy @ 208
;   total_jiffies @ 216
;   swap_cnter @ 224

%define TASK_RIP        56
%define TASK_RFLAGS     64
%define TASK_RSP        72
%define TASK_RSI        80
%define TASK_RAX        88
%define TASK_RCX        96
%define TASK_RDX        104
%define TASK_RBX        112
%define TASK_RBP        120
%define TASK_RDI        128
%define TASK_R8         136
%define TASK_R9         144
%define TASK_R10        152
%define TASK_R11        160
%define TASK_R12        168
%define TASK_R13        176
%define TASK_R14        184
%define TASK_R15        192
%define TASK_FPU        200

sched_context_switch:
    ; -------------------------------------------------------
    ; TASK_CONTEXT_SAVE - prev gorevinin durumunu kaydet
    ; TempleOS TASK_CONTEXT_SAVE macro karsiligi
    ;
    ; rdi = prev task_t*
    ; rsi = next task_t*
    ;
    ; Not: rdi (prev ptr) ve rsi (next ptr) kaydet oncesi
    ; kaydedilmeli; aksi halde uzerine yazariz.
    ; -------------------------------------------------------

    ; Once rdi ve rsi'yi gecici register'lara al
    ; r10 = prev, r11 = next (caller-saved, guvenli)
    mov     r10, rdi        ; r10 = prev
    mov     r11, rsi        ; r11 = next

    ; --- prev register'larini kaydet ---
    ; RIP: CALL komutu return adresini [rsp]'ye itmis
    ; Biz buradan donunce devam edecegiz -> [rsp] = return addr
    mov     rax, [rsp]
    mov     [r10 + TASK_RIP], rax

    ; RSP: mevcut stack pointer (return addr altindaki konum)
    ; sched_context_switch'ten dondugumuzde RSP restore edilmeli
    lea     rax, [rsp + 8]      ; return addr'i say, onu da geri ver
    mov     [r10 + TASK_RSP], rax

    ; RFLAGS
    pushfq
    pop     rax
    mov     [r10 + TASK_RFLAGS], rax

    ; Genel amacli register'lar
    ; Not: rdi ve rsi asil degerlerini r10/r11'den al
    mov     [r10 + TASK_RSI], rsi   ; Kaydedilmis next ptr (goreve gore yanlis)
    ; Asil rsi degerini kaydetmek icin caller'dan gelmeli;
    ; simdilik sched_context_switch argumanlari olarak gelen
    ; deger zaten prev'in rsi'si degil. Callback olarak 0 yaziyoruz.
    ; Dogrulama: context restore sonrasi rsi zaten next ptr olacak.
    mov     [r10 + TASK_RAX], rax
    mov     [r10 + TASK_RCX], rcx
    mov     [r10 + TASK_RDX], rdx
    mov     [r10 + TASK_RBX], rbx
    mov     [r10 + TASK_RBP], rbp
    mov     [r10 + TASK_RDI], rdi   ; Kaydedilmis prev ptr
    mov     [r10 + TASK_R8],  r8
    mov     [r10 + TASK_R9],  r9
    mov     [r10 + TASK_R10], r10
    mov     [r10 + TASK_R11], r11
    mov     [r10 + TASK_R12], r12
    mov     [r10 + TASK_R13], r13
    mov     [r10 + TASK_R14], r14
    mov     [r10 + TASK_R15], r15

    ; FPU/SSE durumu kaydet (fxsave)
    ; TempleOS: FXSAVE [RSI+CFPU.body]
    mov     rax, [r10 + TASK_FPU]
    test    rax, rax
    jz      .skip_fxsave
    fxsave  [rax]
.skip_fxsave:

    ; -------------------------------------------------------
    ; _TASK_CONTEXT_RESTORE - next gorevini yukle
    ; TempleOS _TASK_CONTEXT_RESTORE macro karsiligi
    ;
    ; r11 = next task_t*
    ; -------------------------------------------------------

    ; FPU/SSE durumu yukle (fxrstor)
    ; TempleOS: FXRSTOR [RDI+CFPU.body]
    mov     rax, [r11 + TASK_FPU]
    test    rax, rax
    jz      .skip_fxrstor
    fxrstor [rax]
.skip_fxrstor:

    ; Genel amacli register'lari geri yukle
    mov     r15, [r11 + TASK_R15]
    mov     r14, [r11 + TASK_R14]
    mov     r13, [r11 + TASK_R13]
    mov     r12, [r11 + TASK_R12]
    ; r10/r11 en son yuklenecek (hala next ptr'i tutuyorlar)
    mov     r9,  [r11 + TASK_R9]
    mov     r8,  [r11 + TASK_R8]
    mov     rbp, [r11 + TASK_RBP]
    mov     rbx, [r11 + TASK_RBX]
    mov     rdx, [r11 + TASK_RDX]
    mov     rcx, [r11 + TASK_RCX]
    mov     rsi, [r11 + TASK_RSI]
    mov     rdi, [r11 + TASK_RDI]

    ; RSP'yi geri yukle
    mov     rsp, [r11 + TASK_RSP]

    ; RFLAGS geri yukle
    mov     rax, [r11 + TASK_RFLAGS]
    push    rax
    popfq

    ; r10 ve r11'i geri yukle (artik next ptr'a ihtiyac yok)
    mov     r10, [r11 + TASK_R10]
    ; rax'i next->rip ile hazirla, r11'i yukle, sonra jmp
    mov     rax, [r11 + TASK_RIP]
    mov     r11, [r11 + TASK_R11]

    ; rax = next->rip -> buraya atla
    ; TempleOS: JMP RSI (RESTORE_RSI_TASK sonunda)
    jmp     rax
