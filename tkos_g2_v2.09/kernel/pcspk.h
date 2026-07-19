#pragma once
#include "types.h"

/*
 * TKOS - PC Speaker Surucusu
 * TempleOS Snd.HC Snd(), Beep(), SndFreq() fonksiyonlarindan
 * uyarlanmistir.
 *
 * Donanim:
 *   PIT Kanal 2 (0x42) -> PC Speaker frekans ureteci
 *   Port 0x61 bit0     -> PIT Kanal 2 gate (aktif/pasif)
 *   Port 0x61 bit1     -> Speaker cikis (acik/kapali)
 *
 * TempleOS Snd.HC ile paralel:
 *   SndFreq(freq)  -> pcspk_play_freq(freq)
 *   Snd(0)         -> pcspk_stop()
 *   Beep()         -> pcspk_beep()
 *   Musik notalar  -> pcspk_play_note(note, octave)
 */

/* PIT Kanal 2 portlari */
#define PCSPK_PIT_CH2   0x42    /* PIT kanal 2 veri portu     */
#define PCSPK_PIT_CMD   0x43    /* PIT komut portu            */
#define PCSPK_CTRL      0x61    /* PC Speaker kontrol portu   */

/* PIT temel frekansi */
#define PCSPK_BASE_FREQ 1193182UL

/*
 * Nota adlari - TempleOS muzik sistemiyle uyumlu
 * Yari sesler (diyez) _S suffix ile gosterilir.
 * NOTE_REST = sessizlik (0 Hz)
 */
typedef enum {
    NOTE_REST = 0,
    NOTE_C,   NOTE_CS,
    NOTE_D,   NOTE_DS,
    NOTE_E,
    NOTE_F,   NOTE_FS,
    NOTE_G,   NOTE_GS,
    NOTE_A,   NOTE_AS,
    NOTE_B,
    NOTE_COUNT
} note_name_t;

/*
 * Frekans tablosu - 4. oktav (A4 = 440 Hz, standart akort)
 * TempleOS muzik sistemiyle ayni frekanslar.
 * Diger oktavlar: oktav farki ile 2'nin kuvveti carpilarak hesaplanir.
 *   oktav 4 = tablo degeri
 *   oktav 5 = tablo * 2
 *   oktav 3 = tablo / 2
 */
static const uint32_t note_freqs[NOTE_COUNT] = {
    0,      /* NOTE_REST  */
    262,    /* NOTE_C  4  */
    277,    /* NOTE_CS 4  */
    294,    /* NOTE_D  4  */
    311,    /* NOTE_DS 4  */
    330,    /* NOTE_E  4  */
    349,    /* NOTE_F  4  */
    370,    /* NOTE_FS 4  */
    392,    /* NOTE_G  4  */
    415,    /* NOTE_GS 4  */
    440,    /* NOTE_A  4  */
    466,    /* NOTE_AS 4  */
    494,    /* NOTE_B  4  */
};

/*
 * Melodi nota yapisi
 * TempleOS CMusicNote benzeri:
 *   note    : NOTE_C .. NOTE_B veya NOTE_REST
 *   octave  : 2-7 arasi (4 = orta oktav)
 *   duration: sure (ms cinsinden, PIT_HZ=1000 ile 1:1)
 */
typedef struct {
    note_name_t note;
    uint8_t     octave;
    uint32_t    duration_ms;
} music_note_t;

/* Melodi sonu isaretcisi */
#define MELODY_END  { NOTE_REST, 0, 0 }

/*
 * pcspk_play_freq() - Belirtilen frekansta ses cal.
 * TempleOS SndFreq(freq) karsiligi.
 * 0 Hz verilirse speaker durur.
 *
 * @freq : Hz cinsinden frekans
 */
void pcspk_play_freq(uint32_t freq);

/*
 * pcspk_stop() - Speaker'i sustur.
 * TempleOS Snd(0) karsiligi.
 */
void pcspk_stop(void);

/*
 * pcspk_play_note() - Nota + oktav ile ses cal.
 * TempleOS nota sistemiyle uyumlu frekans hesabi.
 *
 * @note   : NOTE_C .. NOTE_B
 * @octave : 2-7
 */
void pcspk_play_note(note_name_t note, uint8_t octave);

/*
 * pcspk_beep() - Standart sistem bip sesi.
 * TempleOS Beep() karsiligi.
 * 1000 Hz, 100ms.
 */
void pcspk_beep(void);

/*
 * pcspk_play_melody() - Nota dizisi cal (bloklar).
 * PIT tick sayacini kullanarak her notayi bekler.
 * Interrupt'lar acik olmalidir (pit_ticks aktif).
 *
 * @melody : music_note_t dizisi, MELODY_END ile biter
 */
void pcspk_play_melody(const music_note_t *melody);

/*
 * Hazir melodiler
 */
extern const music_note_t melody_boot[];    /* Acilis melodisi  */
extern const music_note_t melody_error[];   /* Hata sesi        */
extern const music_note_t melody_custom[];  /*tema sesi */
