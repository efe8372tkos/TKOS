#pragma once
#include "types.h"



















#define PCSPK_PIT_CH2   0x42    
#define PCSPK_PIT_CMD   0x43    
#define PCSPK_CTRL      0x61    


#define PCSPK_BASE_FREQ 1193182UL






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









static const uint32_t note_freqs[NOTE_COUNT] = {
    0,      
    262,    
    277,    
    294,    
    311,    
    330,    
    349,    
    370,    
    392,    
    415,    
    440,    
    466,    
    494,    
};








typedef struct {
    note_name_t note;
    uint8_t     octave;
    uint32_t    duration_ms;
} music_note_t;


#define MELODY_END  { NOTE_REST, 0, 0 }








void pcspk_play_freq(uint32_t freq);





void pcspk_stop(void);








void pcspk_play_note(note_name_t note, uint8_t octave);






void pcspk_beep(void);








void pcspk_play_melody(const music_note_t *melody);




extern const music_note_t melody_boot[];    
extern const music_note_t melody_error[];   
extern const music_note_t melody_custom[];  
