#include <coco.h>

extern void init_playback();
extern void stop_playback();
extern uint16_t get_freq_value(char *note);
extern void start_voice(uint8_t voice, uint16_t freq);

#define ORG_4VOICE      0x2800

#define init_playback ((void (*)(void)) ORG_4VOICE)
#define stop_playback ((void (*)(void)) (ORG_4VOICE + 3))
#define get_freq_value ((uint16_t (*)(char *)) (ORG_4VOICE + 6))
#define start_voice ((void (*)(uint8_t, uint16_t)) (ORG_4VOICE + 9))
