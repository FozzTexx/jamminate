extern void start_note(uint16_t note);
extern void stop_note(uint16_t note);
extern void stop_all_notes(void);

#define MAX_VOICES 4

/* Note 0 is a real note, so it cannot double as "slot empty". */
#define VOICE_FREE 0xFFFF

extern uint16_t playing[MAX_VOICES];
extern void init_conductor(void);
