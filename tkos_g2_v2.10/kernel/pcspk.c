#include "pcspk.h"
#include "pit.h"
#include "types.h"










static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}












void pcspk_play_freq(uint32_t freq) {
    if (freq == 0) {
        pcspk_stop();
        return;
    }

    uint32_t divisor = (uint32_t)(PCSPK_BASE_FREQ / freq);

    
    outb(PCSPK_PIT_CMD, 0xB6);

    
    outb(PCSPK_PIT_CH2, (uint8_t)(divisor & 0xFF));
    outb(PCSPK_PIT_CH2, (uint8_t)((divisor >> 8) & 0xFF));

    
    uint8_t ctrl = inb(PCSPK_CTRL);
    outb(PCSPK_CTRL, ctrl | 0x03);
}







void pcspk_stop(void) {
    uint8_t ctrl = inb(PCSPK_CTRL);
    outb(PCSPK_CTRL, ctrl & 0xFC);  
}












void pcspk_play_note(note_name_t note, uint8_t octave) {
    if (note == NOTE_REST || note >= NOTE_COUNT) {
        pcspk_stop();
        return;
    }

    uint32_t freq = note_freqs[note];

    
    if (octave > 4) {
        uint8_t shift = octave - 4;
        freq <<= shift;     
    } else if (octave < 4) {
        uint8_t shift = 4 - octave;
        freq >>= shift;     
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





void pcspk_beep(void) {
    pcspk_play_freq(1000);
    pit_sleep_ms(100);
    pcspk_stop();
}











void pcspk_play_melody(const music_note_t *melody) {
    if (!melody) return;

    for (uint32_t i = 0; ; i++) {
        
        if (melody[i].duration_ms == 0 && melody[i].note == NOTE_REST)
            break;

        if (melody[i].note == NOTE_REST) {
            
            pcspk_stop();
        } else {
            pcspk_play_note(melody[i].note, melody[i].octave);
        }

        pit_sleep_ms(melody[i].duration_ms);

        
        pcspk_stop();
        pit_sleep_ms(20);
    }

    pcspk_stop();
}










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





const music_note_t melody_error[] = {
    { NOTE_G, 5, 150 },
    { NOTE_DS, 5, 150 },
    { NOTE_C, 5, 400 },
    MELODY_END
};





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
