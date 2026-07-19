#include "pcspk.h"
#include "pit.h"
#include "types.h"

/*
 * TKOS - PC Speaker Implementasyonu
 * TempleOS Snd.HC SndFreq(), Snd(), Beep() fonksiyonlarindan
 * uyarlanmistir.
 */

/* ------------------------------------------------
 * Port I/O yardimcilari
 * ------------------------------------------------ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ------------------------------------------------
 * pcspk_play_freq()
 * TempleOS SndFreq(freq) ile birebir ayni mantik:
 *
 *   OutU8(0x43, 0xB6)          ; Kanal 2, Mod 3, binary
 *   tmp = 1193180 / freq
 *   OutU8(0x42, tmp & 0xFF)    ; Dusuk byte
 *   OutU8(0x42, tmp >> 8)      ; Yuksek byte
 *   tmp2 = InU8(0x61)
 *   OutU8(0x61, tmp2 | 3)      ; Gate + speaker ac
 * ------------------------------------------------ */
void pcspk_play_freq(uint32_t freq) {
    if (freq == 0) {
        pcspk_stop();
        return;
    }

    uint32_t divisor = (uint32_t)(PCSPK_BASE_FREQ / freq);

    /* PIT Kanal 2: Mod 3 (kare dalga), binary */
    outb(PCSPK_PIT_CMD, 0xB6);

    /* Bolme degerini gonder */
    outb(PCSPK_PIT_CH2, (uint8_t)(divisor & 0xFF));
    outb(PCSPK_PIT_CH2, (uint8_t)((divisor >> 8) & 0xFF));

    /* Speaker'i ac: bit0 (PIT gate) + bit1 (speaker out) */
    uint8_t ctrl = inb(PCSPK_CTRL);
    outb(PCSPK_CTRL, ctrl | 0x03);
}

/* ------------------------------------------------
 * pcspk_stop()
 * TempleOS Snd(0) karsiligi:
 *   tmp = InU8(0x61)
 *   OutU8(0x61, tmp & ~3)   ; Gate + speaker kapat
 * ------------------------------------------------ */
void pcspk_stop(void) {
    uint8_t ctrl = inb(PCSPK_CTRL);
    outb(PCSPK_CTRL, ctrl & 0xFC);  /* bit0 ve bit1 sifirla */
}

/* ------------------------------------------------
 * pcspk_play_note()
 * Nota + oktav -> frekans hesabi.
 *
 * Oktav kaydirma:
 *   Her oktav yukari = frekans * 2
 *   Her oktav asagi  = frekans / 2
 *   Referans: oktav 4 (note_freqs tablosu)
 *
 * TempleOS muzik sistemiyle ayni oktav mantigi.
 * ------------------------------------------------ */
void pcspk_play_note(note_name_t note, uint8_t octave) {
    if (note == NOTE_REST || note >= NOTE_COUNT) {
        pcspk_stop();
        return;
    }

    uint32_t freq = note_freqs[note];

    /* Oktav 4 referans; yukari veya asagi kaydir */
    if (octave > 4) {
        uint8_t shift = octave - 4;
        freq <<= shift;     /* * 2^shift */
    } else if (octave < 4) {
        uint8_t shift = 4 - octave;
        freq >>= shift;     /* / 2^shift */
    }

#ifdef PCSPK_DEBUG_NOTES
    con_print("note=");
    con_print_dec(note);
    con_print(" oct=");
    con_print_dec(octave);
    con_print(" freq=");
    con_print_dec(freq);
    con_print("\n");
#endif

    pcspk_play_freq(freq);
}

/* ------------------------------------------------
 * pcspk_beep()
 * TempleOS Beep() karsiligi: 1000 Hz, 100ms
 * ------------------------------------------------ */
void pcspk_beep(void) {
    pcspk_play_freq(1000);
    pit_sleep_ms(100);
    pcspk_stop();
}

/* ------------------------------------------------
 * pcspk_play_melody()
 * Nota dizisini sirayla calar.
 * Her nota: pcspk_play_note() + pit_sleep_ms() + pcspk_stop()
 *
 * TempleOS muzik calarken:
 *   SndFreq(freq) -> Busy(duration) -> Snd(0)
 * Biz:
 *   pcspk_play_note() -> pit_sleep_ms() -> pcspk_stop()
 * ------------------------------------------------ */
void pcspk_play_melody(const music_note_t *melody) {
    if (!melody) return;

    for (uint32_t i = 0; ; i++) {
        /* MELODY_END kontrolu: duration=0 ve note=REST */
        if (melody[i].duration_ms == 0 && melody[i].note == NOTE_REST)
            break;

        if (melody[i].note == NOTE_REST) {
            /* Sessizlik: speaker sustur, sadece bekle */
            pcspk_stop();
        } else {
            pcspk_play_note(melody[i].note, melody[i].octave);
        }

        pit_sleep_ms(melody[i].duration_ms);

        /* Notalar arasi kisa sessizlik (legato degil) */
        pcspk_stop();
        pit_sleep_ms(20);
    }

    pcspk_stop();
}

/* ------------------------------------------------
 * Hazir melodiler
 * TempleOS acilis muzigi gibi kisa, anlamlı sekanslar.
 * ------------------------------------------------ */

/*
 * Acilis melodisi - sistem hazir oldugunda calar.
 * MIDI'den cevrilmis melodi.
 */
const music_note_t melody_boot[] = {
    { NOTE_C, 5, 96 },
    { NOTE_D, 5, 1335 },
    { NOTE_E, 5, 79 },
    { NOTE_F, 5, 62 },
    { NOTE_E, 5, 85 },
    { NOTE_D, 5, 1545 },
    { NOTE_B, 4, 79 },
    { NOTE_C, 5, 96 },
    { NOTE_B, 4, 79 },
    { NOTE_A, 4, 1221 },
    { NOTE_F, 4, 73 },
    { NOTE_G, 4, 96 },
    { NOTE_F, 4, 79 },
    { NOTE_E, 4, 562 },
    { NOTE_D, 4, 79 },
    { NOTE_C, 4, 62 },
    { NOTE_D, 4, 2829 },
    { NOTE_C, 5, 85 },
    { NOTE_D, 5, 1460 },
    { NOTE_E, 5, 79 },
    { NOTE_F, 5, 96 },
    { NOTE_E, 5, 96 },
    { NOTE_D, 5, 1380 },
    { NOTE_B, 4, 85 },
    { NOTE_C, 5, 85 },
    { NOTE_B, 4, 85 },
    { NOTE_A, 4, 1153 },
    { NOTE_F, 4, 79 },
    { NOTE_G, 4, 85 },
    { NOTE_F, 4, 85 },
    { NOTE_E, 4, 539 },
    { NOTE_D, 4, 45 },
    { NOTE_C, 4, 79 },
    { NOTE_D, 4, 2647 },
    { NOTE_D, 4, 96 },
    { NOTE_E, 4, 96 },
    { NOTE_F, 4, 102 },
    { NOTE_G, 4, 96 },
    { NOTE_AS, 4, 113 },
    { NOTE_A, 4, 113 },
    { NOTE_G, 4, 96 },
    { NOTE_AS, 4, 119 },
    { NOTE_A, 4, 1363 },
    { NOTE_C, 5, 96 },
    { NOTE_B, 4, 1494 },
    { NOTE_D, 5, 113 },
    { NOTE_C, 5, 1414 },
    { NOTE_E, 5, 79 },
    { NOTE_D, 5, 573 },
    { NOTE_F, 5, 113 },
    { NOTE_E, 5, 613 },
    { NOTE_G, 5, 113 },
    { NOTE_F, 5, 556 },
    { NOTE_A, 5, 130 },
    { NOTE_G, 5, 556 },
    { NOTE_B, 5, 113 },
    { NOTE_A, 5, 1778 },
    { NOTE_C, 6, 85 },
    { NOTE_B, 5, 1511 },
    { NOTE_D, 6, 96 },
    { NOTE_C, 6, 1215 },
    MELODY_END
}; 

/*
 * Hata melodisi - kernel panic veya kritik hata.
 * Alcalan uc nota: "bir seyler yanlis gitti".
 */
const music_note_t melody_error[] = {
    { NOTE_G, 5, 150 },
    { NOTE_DS, 5, 150 },
    { NOTE_C, 5, 400 },
    MELODY_END
};

/*
 * Ozel melodi - shell'den "play" komutuyla calinir.
 * MIDI'den cevrilmis melodi.
 */
const music_note_t melody_custom[] = {
    { NOTE_A, 4, 119 },
    { NOTE_A, 4, 119 },
    { NOTE_A, 4, 119 },
    { NOTE_C, 5, 119 },
    { NOTE_C, 5, 119 },
    { NOTE_C, 5, 119 },
    { NOTE_A, 4, 135 },
    { NOTE_E, 5, 135 },
    { NOTE_C, 5, 135 },
    { NOTE_A, 4, 180 },
    { NOTE_A, 3, 105 },
    { NOTE_C, 5, 135 },
    { NOTE_D, 5, 89 },
    { NOTE_D, 5, 89 },
    { NOTE_A, 4, 105 },
    { NOTE_D, 5, 105 },
    { NOTE_E, 5, 150 },
    { NOTE_D, 5, 375 },
    { NOTE_C, 5, 150 },
    { NOTE_B, 4, 300 },
    { NOTE_C, 5, 89 },
    { NOTE_A, 4, 119 },
    { NOTE_A, 4, 105 },
    { NOTE_A, 4, 105 },
    { NOTE_C, 5, 119 },
    { NOTE_C, 5, 119 },
    { NOTE_C, 5, 119 },
    { NOTE_A, 4, 119 },
    { NOTE_E, 5, 119 },
    { NOTE_C, 5, 119 },
    { NOTE_A, 4, 180 },
    { NOTE_A, 3, 89 },
    { NOTE_C, 5, 119 },
    { NOTE_D, 5, 89 },
    { NOTE_D, 5, 89 },
    { NOTE_A, 4, 105 },
    { NOTE_D, 5, 105 },
    { NOTE_E, 5, 164 },
    { NOTE_D, 5, 434 },
    { NOTE_C, 5, 164 },
    { NOTE_B, 4, 300 },
    { NOTE_C, 5, 89 },
    { NOTE_G, 4, 105 },
    { NOTE_AS, 3, 105 },
    { NOTE_E, 5, 119 },
    { NOTE_G, 5, 105 },
    { NOTE_G, 5, 209 },
    { NOTE_C, 4, 135 },
    { NOTE_C, 5, 164 },
    { NOTE_B, 4, 180 },
    { NOTE_E, 5, 105 },
    { NOTE_C, 5, 105 },
    { NOTE_E, 5, 89 },
    { NOTE_C, 5, 89 },
    { NOTE_C, 5, 269 },
    { NOTE_E, 4, 119 },
    { NOTE_E, 5, 150 },
    { NOTE_C, 5, 150 },
    { NOTE_C, 3, 89 },
    { NOTE_E, 5, 150 },
    { NOTE_E, 4, 89 },
    { NOTE_G, 5, 89 },
    { NOTE_G, 5, 89 },
    { NOTE_G, 5, 225 },
    { NOTE_C, 4, 150 },
    { NOTE_C, 5, 180 },
    { NOTE_B, 4, 164 },
    { NOTE_E, 5, 89 },
    { NOTE_C, 5, 119 },
    { NOTE_E, 5, 105 },
    { NOTE_C, 5, 89 },
    { NOTE_G, 5, 135 },
    { NOTE_E, 4, 119 },
    { NOTE_E, 5, 150 },
    { NOTE_A, 3, 105 },
    { NOTE_A, 3, 119 },
    { NOTE_A, 4, 105 },
    { NOTE_C, 5, 135 },
    { NOTE_C, 5, 119 },
    { NOTE_C, 5, 150 },
    { NOTE_A, 4, 119 },
    { NOTE_E, 5, 119 },
    { NOTE_C, 5, 119 },
    { NOTE_A, 4, 164 },
    { NOTE_A, 3, 89 },
    { NOTE_C, 5, 119 },
    { NOTE_D, 5, 89 },
    { NOTE_A, 4, 89 },
    { NOTE_D, 5, 105 },
    { NOTE_E, 5, 164 },
    { NOTE_D, 5, 434 },
    { NOTE_C, 5, 164 },
    { NOTE_B, 4, 300 },
    { NOTE_C, 5, 105 },
    { NOTE_A, 4, 119 },
    { NOTE_A, 4, 119 },
    { NOTE_A, 4, 119 },
    { NOTE_C, 5, 135 },
    { NOTE_C, 5, 119 },
    { NOTE_C, 5, 119 },
    { NOTE_A, 4, 119 },
    { NOTE_E, 5, 119 },
    { NOTE_C, 5, 135 },
    { NOTE_A, 4, 194 },
    { NOTE_A, 3, 89 },
    { NOTE_C, 5, 119 },
    { NOTE_D, 5, 89 },
    { NOTE_D, 5, 89 },
    { NOTE_A, 4, 105 },
    { NOTE_D, 5, 89 },
    { NOTE_E, 5, 164 },
    { NOTE_D, 5, 89 },
    { NOTE_D, 5, 314 },
    { NOTE_C, 5, 164 },
    { NOTE_B, 4, 284 },
    { NOTE_C, 5, 105 },
    { NOTE_G, 4, 119 },
    { NOTE_AS, 3, 119 },
    { NOTE_E, 5, 119 },
    { NOTE_G, 5, 89 },
    { NOTE_G, 5, 89 },
    { NOTE_G, 5, 225 },
    { NOTE_C, 4, 150 },
    { NOTE_C, 5, 164 },
    { NOTE_B, 4, 164 },
    { NOTE_E, 5, 105 },
    { NOTE_C, 5, 105 },
    { NOTE_E, 5, 89 },
    { NOTE_C, 5, 89 },
    { NOTE_C, 5, 269 },
    { NOTE_E, 4, 119 },
    { NOTE_E, 5, 150 },
    { NOTE_C, 5, 135 },
    { NOTE_E, 5, 150 },
    { NOTE_G, 5, 89 },
    { NOTE_G, 5, 89 },
    { NOTE_G, 5, 209 },
    { NOTE_C, 4, 150 },
    { NOTE_C, 5, 164 },
    { NOTE_B, 4, 164 },
    { NOTE_E, 5, 89 },
    { NOTE_C, 5, 119 },
    { NOTE_E, 5, 105 },
    { NOTE_C, 5, 89 },
    { NOTE_G, 5, 135 },
    { NOTE_E, 4, 119 },
    { NOTE_E, 5, 150 },
    MELODY_END
};
