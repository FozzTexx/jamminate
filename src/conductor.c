#include "coco/4voice.h"
#include "coco/graphics.h"
#include "conductor.h"

static char note_str[10];
uint16_t playing[MAX_VOICES];
static const char *note_name[] = {
  "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B ",
};

void init_conductor(void)
{
  uint8_t voice;

  for (voice = 0; voice < MAX_VOICES; voice++)
    playing[voice] = VOICE_FREE;

  return;
}

void start_note(uint16_t note)
{
  uint16_t octave, semi, freq;
  uint8_t voice;


  octave = note / 12;
  semi = note % 12;

  sprintf(note_str, "%s%u", note_name[semi], octave);
  freq = get_freq_value(note_str);

  /* 4voice returns 1 when the note is not in its table -- its own header
     comment claims 0, but the code does LDD #1. A frequency of 0 is what
     *stops* a voice, so 1 is not silence: left alone it would take a voice,
     light that key, and sit there as a subsonic phase increment with nothing
     audible. Reject it here instead. */
  if (freq <= 1) {
    graphics_warn(WARN_NO_FREQ);
    return;
  }

  for (voice = 0; voice < MAX_VOICES; voice++) {
    if (playing[voice] == VOICE_FREE)
      break;
  }
  if (voice == MAX_VOICES)
    return;

  playing[voice] = note;
  start_voice(voice, freq);
  update_key_highlight();

  return;
}

void stop_note(uint16_t note)
{
  uint8_t voice;


  for (voice = 0; voice < MAX_VOICES; voice++) {
    if (playing[voice] == note) {
      start_voice(voice, 0); // frequency of 0 "stops" the voice
      playing[voice] = VOICE_FREE;
      update_key_highlight();
      break;
    }
  }

  return;
}

/* Release every sounding voice. A client that vanishes mid-chord never sends
   its note-OFFs, and those notes would otherwise sound forever with their keys
   lit. */
void stop_all_notes(void)
{
  uint8_t voice;

  for (voice = 0; voice < MAX_VOICES; voice++) {
    if (playing[voice] != VOICE_FREE)
      stop_note(playing[voice]);
  }

  return;
}
