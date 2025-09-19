#include <coco.h>

extern void stop_playback();
extern void init_dac();
extern void init_interrupts();
extern void restore_interrupts();
extern void sound_off();
extern uint16_t get_freq_value(char *note);
extern void start_voice(uint8_t voice, uint16_t freq);

extern uint16_t scratch;
