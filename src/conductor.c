#include "coco/4voice.h"

#define MAX_VOICES 4

static char note_str[10];
static uint16_t playing[MAX_VOICES];
static const char *note_name[] = {
  "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B ",
};

void start_note(uint16_t note)
{
  uint16_t octave, semi, freq;
  uint8_t voice;


  octave = note / 12;
  semi = note % 12;

  sprintf(note_str, "%s%u", note_name[semi], octave);
  freq = get_freq_value(note_str);
  //printf("PLAYING -%s- %04x\n", note_str, freq);
  for (voice = 0; voice < MAX_VOICES; voice++) {
    if (!playing[voice])
      break;
  }
  if (voice == MAX_VOICES)
    return;

  playing[voice] = note;
  start_voice(voice + 1, freq);

  return;
}

void stop_note(uint16_t note)
{
  uint8_t voice;


  for (voice = 0; voice < MAX_VOICES; voice++) {
    if (playing[voice] == note) {
      start_voice(voice + 1, 0); // frequency of 0 "stops" the voice
      playing[voice] = 0;
      break;
    }
  }

  return;
}
