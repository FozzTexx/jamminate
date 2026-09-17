#include <coco.h>
#include "../conductor.h"

/* Fuer Elise, WoO 59 (Beethoven) -- public domain. Chosen because the theme
   sits on D#5 and G#4, so it works the black keys continuously, and because it
   is familiar enough that bad timing is obvious by ear. The tail is not
   Beethoven: an ascending flourish off the top of the keybed and one bass note
   below it, to exercise the out-of-range octave readouts at both ends.

   Each row is four simultaneous notes (0 = unused) then a length in
   sixteenths. DEMO_SIXTEENTH is the only tempo knob, in milliseconds.

   Timing is calibrated for playback, not for an idle machine. The HS sound
   interrupt fires 5,240 times a second and its handler is about 141 cycles
   including entry, so it takes roughly 82% of the 0.895MHz CPU and everything
   here runs at about a sixth speed. cmoc compiles the delay's inner loop to 30
   cycles an iteration, so DELAY_INNER 4 makes one unit of delay_ms() about a
   millisecond under that load. With sound off it runs ~6x faster. */

#define DELAY_INNER 4
#define DEMO_SIXTEENTH 144
#define DEMO_GAP 15

static const uint16_t tune[][5] = {
  {76,  0,  0,  0, 1},   /* E5                      */
  {75,  0,  0,  0, 1},   /* D#5  black              */
  {76,  0,  0,  0, 1},
  {75,  0,  0,  0, 1},   /* D#5  black              */
  {76,  0,  0,  0, 1},
  {71,  0,  0,  0, 1},   /* B4                      */
  {74,  0,  0,  0, 1},   /* D5                      */
  {72,  0,  0,  0, 1},   /* C5                      */
  {69, 45, 52, 57, 3},   /* A4 over A minor         */
  {60,  0,  0,  0, 1},   /* C4                      */
  {64,  0,  0,  0, 1},   /* E4                      */
  {69,  0,  0,  0, 1},   /* A4                      */
  {71, 40, 52, 56, 3},   /* B4 over E, G#3 black    */
  {64,  0,  0,  0, 1},   /* E4                      */
  {68,  0,  0,  0, 1},   /* G#4  black              */
  {71,  0,  0,  0, 1},   /* B4                      */
  {72, 45, 52, 57, 3},   /* C5 over A minor         */
  {64,  0,  0,  0, 1},   /* E4                      */

  {76,  0,  0,  0, 1},   /* theme again             */
  {75,  0,  0,  0, 1},
  {76,  0,  0,  0, 1},
  {75,  0,  0,  0, 1},
  {76,  0,  0,  0, 1},
  {71,  0,  0,  0, 1},
  {74,  0,  0,  0, 1},
  {72,  0,  0,  0, 1},
  {69, 45, 52, 57, 3},
  {60,  0,  0,  0, 1},
  {64,  0,  0,  0, 1},
  {69,  0,  0,  0, 1},
  {71, 40, 52, 56, 3},
  {64,  0,  0,  0, 1},
  {72,  0,  0,  0, 1},   /* C5                      */
  {71,  0,  0,  0, 1},   /* B4                      */
  {69, 45, 52, 57, 6},   /* A4 held, slowing        */

  {69, 24,  0,  0, 4},   /* A2 below the keybed -> 2 at the left   */

  {76,  0,  0,  0, 1},   /* flourish up and off the top            */
  {79,  0,  0,  0, 1},   /* G5                                     */
  {83,  0,  0,  0, 1},   /* B5, the top key                        */
  {84,  0,  0,  0, 2},   /* C6 above the keybed -> 7 at the right  */
  {88,  0,  0,  0, 2},   /* E6                                     */
  {96,  0,  0,  0, 3},   /* C7 -> 8 at the right                   */
  {96, 88, 84, 69, 8},   /* highest sounding is 96, so 8; A4 still
                            lights its own key and marker          */
};

#define TUNE_STEPS (sizeof(tune) / sizeof(tune[0]))

static void delay_ms(uint16_t ms)
{
  uint16_t i, j;

  for (i = 0; i < ms; i++)
    for (j = 0; j < DELAY_INNER; j++)
      ;
}

void play_demo(void)
{
  uint8_t i, j;

  for (i = 0; i < TUNE_STEPS; i++) {
    for (j = 0; j < 4; j++)
      if (tune[i][j])
        start_note(tune[i][j]);

    delay_ms(tune[i][4] * DEMO_SIXTEENTH - DEMO_GAP);

    for (j = 0; j < 4; j++)
      if (tune[i][j])
        stop_note(tune[i][j]);

    delay_ms(DEMO_GAP);

    if (inkey())
      return;
  }
}
