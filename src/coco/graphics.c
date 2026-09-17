#include <coco.h>
#include "../conductor.h"
#include "font.h"
#include "graphics.h"

#define WHITE 1
#define BLACK 0
#define WHITE_BYTE 0xFF

#define SCREEN_W 256
#define SCREEN_H 192
#define SCREEN_STRIDE (SCREEN_W / 8)

/* Color Basic keeps the graphics RAM page number at $BC; the page number
   shifted up a byte is the address. Pass an explicit address here instead if
   Basic's PCLEAR area is ever needed for something else. */
#define SCREEN_BUFFER ((uint8_t *) ((uint16_t) *(uint8_t *) 0x00BC << 8))

#define OCTAVES 4

/* Which note the leftmost key is. Must be a multiple of 12 -- the keybed is
   drawn assuming key 0 is a C. 36 puts middle C (60) on white key 14 of 28,
   so the four octaves straddle where playing actually happens instead of
   sitting below it. */
#define DISPLAY_BASE_NOTE 36
#define KEY_COUNT (OCTAVES * 12)
#define WHITE_KEYS (OCTAVES * 7)

#define KEY_PITCH 9
#define KEY_WIDTH 10
#define BLACK_KEY_WIDTH 5
#define KEY_HEIGHT 64
#define BLACK_KEY_HEIGHT 38
#define KEYBOARD_LEFT 1

#define TITLE_SCALE 4
#define TITLE_ADVANCE 26
#define TITLE_LEFT 14
#define TITLE_LEN 9

#define OCTAVE_SCALE 2

/* Out-of-range octave readouts live at the two ends of the octave row, clear
   of the static markers (which span x=27..225). Both start on a byte boundary
   so only these bytes get rewritten and the markers are left alone. Three
   bytes is enough for two scale-2 digits, which octave 10 needs. */
#define EDGE_SLOT_BYTES 3
#define EDGE_LEFT_BYTE 0
#define EDGE_RIGHT_BYTE (SCREEN_STRIDE - EDGE_SLOT_BYTES)
#define EDGE_NONE 0xFF

/* Recoverable trouble shows as a code digit in a small inverted box in the
   bottom right, clear of everything else, and clears itself so it cannot sit
   there misleading anyone. WARN_HOLD is in passes of the network poll loop,
   which is the only clock available without blocking it. */
#define WARN_X 244
#define WARN_BYTE 30
#define WARN_BYTES 2
#define WARN_Y (LETTER_TOP + GLYPH_H + 6)
#define WARN_HOLD 1500

/* The link mark sits at the opposite end of the same row as the warning digit,
   so the two can never overwrite each other. */
#define LINK_X 6
#define LINK_BYTE 0
#define LINK_BYTES 2
#define LINK_Y WARN_Y
#define LINK_H 5

/* Vertical layout stacks down from LAYOUT_TOP -- title, octave digits, keybed,
   note letters -- so sliding the whole block is this one constant. The stack is
   126 rows in a 192-row screen; LAYOUT_TOP 30 leaves 30 above and 36 below,
   which centers it while sitting a shade high. */
#define LAYOUT_TOP 30
#define TITLE_TOP LAYOUT_TOP
#define OCTAVE_TOP (TITLE_TOP + GLYPH_H * TITLE_SCALE + 4)
#define KEYBOARD_TOP (OCTAVE_TOP + GLYPH_H * OCTAVE_SCALE + 4)
#define LETTER_TOP (KEYBOARD_TOP + KEY_HEIGHT + 5)

static const char title_text[] = "JAMMINATE";
static const char white_names[] = "CDEFGAB";

typedef struct {
  uint16_t note;
  uint16_t x;
  uint8_t width;
  uint8_t is_black;
} Key;

static Key keys[KEY_COUNT];
static uint8_t key_count;
static uint8_t *screen_buf;

static uint8_t row_rule[SCREEN_STRIDE];
static uint8_t row_upper[SCREEN_STRIDE];
static uint8_t row_lower[SCREEN_STRIDE];
static uint8_t row_bfoot[SCREEN_STRIDE];
static uint8_t row_foot[SCREEN_STRIDE];
static uint8_t row_scratch[SCREEN_STRIDE];
static uint8_t xor_mask[SCREEN_STRIDE];
static uint8_t lit[KEY_COUNT];
static uint8_t want[KEY_COUNT];
static uint8_t oct_lit[OCTAVES];
static uint8_t want_oct[OCTAVES];
static uint8_t edge_high = EDGE_NONE;
static uint8_t edge_low = EDGE_NONE;
static uint16_t warn_hold;

static const uint8_t is_black_key[12] = {
  0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0
};

/* PMODE4 is one bit per pixel, 32 bytes per row, most significant bit
   leftmost. Under colorset 1 a set bit is white and a clear bit is black.
   coco.h has no PMODE plot call -- hset() belongs to the CoCo 3's HSCREEN
   modes -- so the buffer is written directly, as hirestxt does. */
static void row_plot(uint8_t *row, uint16_t x, uint8_t color)
{
  uint8_t mask;

  if (x >= SCREEN_W)
    return;

  mask = (uint8_t) (0x80 >> (x & 7));

  if (color)
    row[x >> 3] |= mask;
  else
    row[x >> 3] &= (uint8_t) ~mask;
}

static void row_span(uint8_t *row, uint16_t x, uint16_t w, uint8_t color)
{
  uint16_t i;

  for (i = 0; i < w; i++)
    row_plot(row, x + i, color);
}

static void blit_bytes(uint8_t *row, uint8_t first, uint8_t n, uint16_t y,
                       uint16_t count)
{
  uint8_t *p = screen_buf + (uint16_t) y * SCREEN_STRIDE + first;

  while (count--) {
    memcpy(p, row + first, n);
    p += SCREEN_STRIDE;
  }
}

static void blit_rows(uint8_t *row, uint16_t y, uint16_t count)
{
  uint8_t *p = screen_buf + (uint16_t) y * SCREEN_STRIDE;

  while (count--) {
    memcpy(p, row, SCREEN_STRIDE);
    p += SCREEN_STRIDE;
  }
}

static uint8_t font_index(char c)
{
  uint8_t i;

  for (i = 0; i < FONT_COUNT; i++)
    if (font_chars[i] == c)
      return i;

  return FONT_COUNT;
}

static void row_glyph(uint8_t *row, uint16_t x, uint8_t idx, uint8_t grow,
                      uint8_t scale, uint8_t color)
{
  uint8_t bit;

  if (idx >= FONT_COUNT)
    return;

  for (bit = 0; bit < GLYPH_W; bit++)
    if ((font_bitmaps[idx][grow] >> (GLYPH_W - 1 - bit)) & 1)
      row_span(row, x + (uint16_t) bit * scale, scale, color);
}

/* Scaled up from the same 5x7 cells the note letters use, so the title and the
   labels share one typeface. Each font row is built once as a full scanline
   carrying all nine letters and then copied down TITLE_SCALE times; building
   whole scanlines is what lets the letters share a row buffer without one
   erasing the last. */
static void draw_title()
{
  uint8_t row, i;

  for (row = 0; row < GLYPH_H; row++) {
    memset(row_scratch, WHITE_BYTE, SCREEN_STRIDE);

    for (i = 0; i < TITLE_LEN; i++)
      row_glyph(row_scratch, TITLE_LEFT + (uint16_t) i * TITLE_ADVANCE,
                font_index(title_text[i]), row, TITLE_SCALE, BLACK);

    blit_rows(row_scratch, TITLE_TOP + (uint16_t) row * TITLE_SCALE, TITLE_SCALE);
  }
}

/* Every scanline of the keybed is one of five patterns: a solid rule along the
   top edge, the band where the black keys sit over the white-key borders, the
   black keys' own foot, the band below them where only the borders remain, and
   the white keys' foot. Building one 32-byte row of each and copying it down
   costs a fraction of plotting every pixel individually.

   A foot insets its key by a pixel on both sides, so the last border pixel
   sits diagonally off the end of the bottom edge and reads as a rounded front
   corner rather than a square one.

   White keys are outlines that share their side borders, so the dividers read
   as single lines. The black keys are then filled solid on top, covering the
   seam between the two white keys they straddle. */
void draw_keyboard()
{
  uint8_t i;

  memset(row_rule, WHITE_BYTE, SCREEN_STRIDE);
  memset(row_upper, WHITE_BYTE, SCREEN_STRIDE);
  memset(row_lower, WHITE_BYTE, SCREEN_STRIDE);
  memset(row_bfoot, WHITE_BYTE, SCREEN_STRIDE);
  memset(row_foot, WHITE_BYTE, SCREEN_STRIDE);

  for (i = 0; i < key_count; i++) {
    if (keys[i].is_black)
      continue;
    row_span(row_rule, keys[i].x, KEY_WIDTH, BLACK);
    row_plot(row_upper, keys[i].x, BLACK);
    row_plot(row_upper, keys[i].x + KEY_WIDTH - 1, BLACK);
    row_plot(row_lower, keys[i].x, BLACK);
    row_plot(row_lower, keys[i].x + KEY_WIDTH - 1, BLACK);
    row_plot(row_bfoot, keys[i].x, BLACK);
    row_plot(row_bfoot, keys[i].x + KEY_WIDTH - 1, BLACK);
    row_span(row_foot, keys[i].x + 1, KEY_WIDTH - 2, BLACK);
  }

  for (i = 0; i < key_count; i++)
    if (keys[i].is_black) {
      row_span(row_upper, keys[i].x, keys[i].width, BLACK);
      row_span(row_bfoot, keys[i].x + 1, BLACK_KEY_WIDTH - 2, BLACK);
    }

  blit_rows(row_rule, KEYBOARD_TOP, 1);
  blit_rows(row_upper, KEYBOARD_TOP + 1, BLACK_KEY_HEIGHT - 2);
  blit_rows(row_bfoot, KEYBOARD_TOP + BLACK_KEY_HEIGHT - 1, 1);
  blit_rows(row_lower, KEYBOARD_TOP + BLACK_KEY_HEIGHT,
            KEY_HEIGHT - BLACK_KEY_HEIGHT - 1);
  blit_rows(row_foot, KEYBOARD_TOP + KEY_HEIGHT - 1, 1);
}


/* The octave digit sits above the keybed, centered over its seven white keys,
   and the note letter below each white key. Both are on blank background
   rather than on the keys, so a highlighted key can be filled solid without
   swallowing a glyph -- and this no longer depends on draw_keyboard() having
   built the border pattern. Per-key letters are stuck at scale 1: KEY_PITCH is
   9, so a scale-2 glyph would be 10 wide and neighbors would collide. */
static uint16_t octave_label_x(uint8_t o)
{
  uint16_t first = KEYBOARD_LEFT + (uint16_t) o * 7 * KEY_PITCH;
  uint16_t last = first + 6 * KEY_PITCH + KEY_WIDTH - 1;

  return (first + last) / 2 - GLYPH_W * OCTAVE_SCALE / 2;
}

static void draw_labels()
{
  uint8_t grow, i, w, o;

  for (grow = 0; grow < GLYPH_H; grow++) {
    memset(row_scratch, WHITE_BYTE, SCREEN_STRIDE);

    for (i = 0, w = 0; i < key_count; i++) {
      if (keys[i].is_black)
        continue;

      row_glyph(row_scratch, keys[i].x + (KEY_WIDTH - GLYPH_W) / 2,
                font_index(white_names[w % 7]), grow, 1, BLACK);
      w++;
    }

    blit_rows(row_scratch, LETTER_TOP + grow, 1);
  }

  for (grow = 0; grow < GLYPH_H; grow++) {
    memset(row_scratch, WHITE_BYTE, SCREEN_STRIDE);

    for (o = 0; o < OCTAVES; o++)
      row_glyph(row_scratch, octave_label_x(o),
                font_index((char) ('0' + DISPLAY_BASE_NOTE / 12 + o)),
                grow, OCTAVE_SCALE, BLACK);

    blit_rows(row_scratch, OCTAVE_TOP + (uint16_t) grow * OCTAVE_SCALE,
              OCTAVE_SCALE);
  }
}

void init_graphics()
{
  uint16_t note;
  uint8_t white_index = 0;

  /* Order matches hirestxt's known-good sequence: PMODE graphics only appear
     from a 32-column text mode, pmode() sets the page, screen() displays it. */
  width(32);
  pmode(4, SCREEN_BUFFER);
  pcls(WHITE_BYTE);
  screen(1, 1);

  screen_buf = SCREEN_BUFFER;

  key_count = 0;
  for (note = 0; note < KEY_COUNT; note++) {
    uint8_t semi = (uint8_t) (note % 12);

    keys[note].note = DISPLAY_BASE_NOTE + note;
    keys[note].is_black = is_black_key[semi];

    if (is_black_key[semi]) {
      /* Centered on the seam at the left edge of the white key that follows.
         The width must stay odd to sit symmetrically on that single-pixel
         seam, and wide enough that insetting the foot leaves more than the
         seam column itself -- otherwise the rounding is invisible. */
      keys[note].width = BLACK_KEY_WIDTH;
      keys[note].x = KEYBOARD_LEFT + (uint16_t) white_index * KEY_PITCH
                     - BLACK_KEY_WIDTH / 2;
    } else {
      keys[note].width = KEY_WIDTH;
      keys[note].x = KEYBOARD_LEFT + (uint16_t) white_index * KEY_PITCH;
      white_index++;
    }

    key_count++;
  }

  draw_title();
  draw_keyboard();
  draw_labels();
}

/* Notes outside the keybed fold onto the nearest key of the same pitch class:
   down to the top octave, up to the bottom one. Several sounding notes can
   therefore share a key, which is why the highlight is recomputed from
   playing[] rather than driven by whichever note just started or stopped. */
static uint8_t key_for_note(uint16_t note)
{
  uint8_t semi;

  if (note >= DISPLAY_BASE_NOTE && note < DISPLAY_BASE_NOTE + KEY_COUNT)
    return (uint8_t) (note - DISPLAY_BASE_NOTE);

  semi = (uint8_t) (note % 12);

  if (note < DISPLAY_BASE_NOTE)
    return semi;

  return (uint8_t) (KEY_COUNT - 12 + semi);
}

/* An XOR, so pressing and releasing are the same call and no font or keybed
   pattern has to be redrawn. The masks are the same on every row of the
   column, so they are built once and then applied down it -- a couple of
   read-modify-writes per row instead of one per pixel. */
static void xor_column(uint16_t x, uint16_t w, uint16_t y, uint16_t h)
{
  uint8_t first = (uint8_t) (x >> 3);
  uint8_t last = (uint8_t) ((x + w - 1) >> 3);
  uint8_t b;
  uint16_t i;
  uint8_t *p;

  for (b = first; b <= last; b++)
    xor_mask[b] = 0;

  for (i = 0; i < w; i++)
    xor_mask[(x + i) >> 3] |= (uint8_t) (0x80 >> ((x + i) & 7));

  p = screen_buf + (uint16_t) y * SCREEN_STRIDE;

  while (h--) {
    for (b = first; b <= last; b++)
      p[b] ^= xor_mask[b];
    p += SCREEN_STRIDE;
  }
}

/* Inverts the whole key, stopping a pixel short of everything that bounds it
   so a lit key stays readable: short of its own outline, so two lit neighbors
   do not merge across their shared divider; short of any black key intruding
   on it, so a lit white key is distinct from an unlit black one; and short of
   the rounded foot, so the key keeps its shape. An intruding black key covers
   exactly three pixels of the interior -- X..X+2 on the left, X+7..X+9 on the
   right -- and C and F have none on their left while E and B have none on
   their right, so each key lights in its real piano shape: a narrow neck
   between the black keys widening to a full front. The note letter inverts
   with it; black keys have no letter. */
static void toggle_key(uint8_t i)
{
  uint8_t semi;
  uint16_t x0, x1;

  if (keys[i].is_black) {
    xor_column(keys[i].x + 1, BLACK_KEY_WIDTH - 2,
               KEYBOARD_TOP + 1, BLACK_KEY_HEIGHT - 2);
    return;
  }

  semi = (uint8_t) (keys[i].note % 12);
  x0 = keys[i].x + (is_black_key[(semi + 11) % 12] ? 4 : 2);
  x1 = keys[i].x + (is_black_key[(semi + 1) % 12] ? 5 : 7);

  xor_column(x0, x1 - x0 + 1, KEYBOARD_TOP + 1, BLACK_KEY_HEIGHT - 1);
  xor_column(keys[i].x + 2, KEY_WIDTH - 4,
             KEYBOARD_TOP + BLACK_KEY_HEIGHT,
             KEY_HEIGHT - BLACK_KEY_HEIGHT - 1);
  xor_column(keys[i].x + (KEY_WIDTH - GLYPH_W) / 2 - 1, GLYPH_W + 2,
             LETTER_TOP - 1, GLYPH_H + 2);
}

/* The real octave of a folded note, shown only while one is sounding: above
   the range at the right end of the octave row, below it at the left. The
   static markers only name the four octaves on the keybed, so a folded note
   would otherwise light a marker for an octave it is not in. */
static void draw_edge_octave(uint8_t slot, uint8_t octave)
{
  uint8_t grow, i, n;
  char digits[2];
  uint16_t x;

  n = 0;
  if (octave != EDGE_NONE) {
    if (octave >= 10)
      digits[n++] = (char) ('0' + octave / 10);
    digits[n++] = (char) ('0' + octave % 10);
  }

  for (grow = 0; grow < GLYPH_H; grow++) {
    memset(row_scratch, WHITE_BYTE, SCREEN_STRIDE);

    x = (uint16_t) slot * 8 + 1;
    for (i = 0; i < n; i++) {
      row_glyph(row_scratch, x, font_index(digits[i]), grow, OCTAVE_SCALE,
                BLACK);
      x += GLYPH_W * OCTAVE_SCALE + 1;
    }

    blit_bytes(row_scratch, slot, EDGE_SLOT_BYTES,
               OCTAVE_TOP + (uint16_t) grow * OCTAVE_SCALE, OCTAVE_SCALE);
  }
}

void update_key_highlight(void)
{
  uint8_t i, v, o, k, hi, lo;
  uint16_t note;

  for (i = 0; i < KEY_COUNT; i++)
    want[i] = 0;
  for (o = 0; o < OCTAVES; o++)
    want_oct[o] = 0;

  hi = EDGE_NONE;
  lo = EDGE_NONE;

  for (v = 0; v < MAX_VOICES; v++) {
    if (playing[v] == VOICE_FREE)
      continue;

    note = playing[v];
    k = key_for_note(note);
    want[k] = 1;

    o = (uint8_t) (note / 12);

    if (note >= DISPLAY_BASE_NOTE + KEY_COUNT) {
      if (hi == EDGE_NONE || o > hi)
        hi = o;
    }
    else if (note < DISPLAY_BASE_NOTE) {
      if (lo == EDGE_NONE || o < lo)
        lo = o;
    }
    else
      want_oct[k / 12] = 1;
  }

  for (i = 0; i < KEY_COUNT; i++)
    if (want[i] != lit[i]) {
      toggle_key(i);
      lit[i] = want[i];
    }

  for (o = 0; o < OCTAVES; o++)
    if (want_oct[o] != oct_lit[o]) {
      xor_column(octave_label_x(o) - 1, GLYPH_W * OCTAVE_SCALE + 2,
                 OCTAVE_TOP - 1, GLYPH_H * OCTAVE_SCALE + 2);
      oct_lit[o] = want_oct[o];
    }

  if (hi != edge_high) {
    draw_edge_octave(EDGE_RIGHT_BYTE, hi);
    edge_high = hi;
  }

  if (lo != edge_low) {
    draw_edge_octave(EDGE_LEFT_BYTE, lo);
    edge_low = lo;
  }
}

static void draw_warning(uint8_t code)
{
  uint8_t grow;

  if (!screen_buf)
    return;

  for (grow = 0; grow < GLYPH_H + 2; grow++) {
    memset(row_scratch, WHITE_BYTE, SCREEN_STRIDE);

    if (code != WARN_NONE) {
      row_span(row_scratch, WARN_X - 1, GLYPH_W + 2, BLACK);
      if (grow >= 1 && grow <= GLYPH_H)
        row_glyph(row_scratch, WARN_X, font_index((char) ('0' + code)),
                  (uint8_t) (grow - 1), 1, WHITE);
    }

    blit_bytes(row_scratch, WARN_BYTE, WARN_BYTES, WARN_Y + grow, 1);
  }
}

/* A small x, shown only while nothing is connected, so an idle instrument is
   distinguishable from a wedged one. Absent during play.

   Drawn as its own two diagonals rather than from the font, which carries only
   the note letters, digits and the title -- there is no 'x' in it. */
void graphics_link(uint8_t online)
{
  uint8_t grow;

  if (!screen_buf)
    return;

  for (grow = 0; grow < GLYPH_H + 2; grow++) {
    memset(row_scratch, WHITE_BYTE, SCREEN_STRIDE);

    if (!online && grow >= 2 && grow < 2 + LINK_H) {
      uint8_t d = (uint8_t) (grow - 2);

      row_plot(row_scratch, LINK_X + d, BLACK);
      row_plot(row_scratch, LINK_X + (LINK_H - 1) - d, BLACK);
    }

    blit_bytes(row_scratch, LINK_BYTE, LINK_BYTES, LINK_Y + grow, 1);
  }
}

void graphics_warn(uint8_t code)
{
  warn_hold = WARN_HOLD;
  draw_warning(code);
}

void graphics_warn_tick(void)
{
  if (!warn_hold)
    return;

  if (--warn_hold == 0)
    draw_warning(WARN_NONE);
}

/* Leave graphics mode, and nothing else. Clearing is the caller's decision and
   has to come later: the caller unwinds sound and the network first, and only
   then clears and prints, so nothing the unwind leaves behind can sit under
   the message. */
void end_graphics(void)
{
  screen(0, 0);
}
