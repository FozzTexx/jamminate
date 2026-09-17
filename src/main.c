#include "coco/4voice.h"
#include "coco/serial.h"
#include "coco/graphics.h"
#include "coco/demo.h"
#include "conductor.h"

#include <fujinet-fuji.h>
#include <fujinet-network.h>

#define PORT "6573"
#define SOCKET "N:TCP://:" PORT
#define FRAMING 0xC0

#define PIA2_REG0 ((uint8_t *) 0xFF20)
#define PIA2_REG1 (PIA2_REG0 + 1)

extern void play_string(const char *notes);

AdapterConfigExtended ace;

uint8_t buffer[256];

#define COLUMNS 6

void hexdump(uint8_t *buffer, uint16_t count)
{
  uint16_t outer, inner;
  uint8_t c;


  for (outer = 0; outer < count; outer += COLUMNS) {
    for (inner = 0; inner < COLUMNS; inner++) {
      if (inner + outer < count) {
	c = buffer[inner + outer];
	printf("%02x ", c);
      }
      else
	printf("   ");
    }
    printf(" |");
    for (inner = 0; inner < COLUMNS && inner + outer < count; inner++) {
      c = buffer[inner + outer];
      if (c >= ' ' && c <= 0x7f)
	printf("%c", c);
      else
	printf(".");
    }
    printf("|\n");
  }

  return;
}

uint16_t parse_osc_message(uint8_t *msg, uint16_t msglen,
                           char **osc_addr, char **osc_types, char **osc_values)
{
  uint8_t *addr, *types, *values;
  uint16_t pos, offset;


  *osc_addr = *osc_types = *osc_values = NULL;

  // Find first 0xC0
  for (offset = 0; offset < msglen && msg[offset] != FRAMING; offset++)
    ;
  if (offset >= msglen || msg[offset] != FRAMING) {
    return msglen;
  }
  offset++;

  /* SLIP allows a run of delimiters between frames. Skip them, or a frame
     preceded by one reads as malformed and its note is lost. */
  while (offset < msglen && msg[offset] == FRAMING)
    offset++;

  if (offset >= msglen || msg[offset] != '/')
    return msglen;

  addr = &msg[offset];

  // Find null terminator at end of OSC address
  for (pos = 0; offset + pos < msglen && msg[offset + pos]; pos++)
    ;
  if (offset + pos >= msglen || msg[offset + pos]) {
    return msglen;
  }

  /* Skip the padding. An OSC field is padded to a multiple of 4 bytes
     INCLUDING its terminating null, so the round-up is over pos + 1, not pos.
     Using pos alone happens to give the right answer for a 2- or 3-character
     address and lands on the null itself for a 4-character one, which made
     every note from 100 up unparseable. */
  pos = ((pos + 4) / 4) * 4;
  if (offset + pos >= msglen || msg[offset + pos] != ',') {
    return msglen;
  }

  types = &msg[offset + pos];

  // Find null terminator at end of OSC types
  for (; offset + pos < msglen && msg[offset + pos]; pos++)
    ;
  if (offset + pos >= msglen || msg[offset + pos]) {
    return msglen;
  }

  pos = ((pos + 4) / 4) * 4;
  if (offset + pos >= msglen) {
    return msglen;
  }

  values = &msg[offset + pos];

  // Move past 0xC0 at end of packet
  for (; offset + pos < msglen && msg[offset + pos] != FRAMING; pos++)
    ;
  if (msg[offset + pos] == FRAMING)
    pos++;

  *osc_addr = (char *) addr;
  *osc_types = (char *) types;
  *osc_values = (char *) values;

  return offset + pos;
}

#ifdef OBSOLETE
void setup_cd_flag()
{
  uint8_t val, prev;


  val = *PIA2_REG1;
  val &= 0xFC; // Disable FIRQ, flag on falling edge (CD off to on)
  val |= 2; // flag on rising edge (CD on to off)
  *PIA2_REG1 = val;
  (void) *PIA2_REG0; // Clear the CD flag}

#if 0
  for (prev = 0;; prev = val) {
    val = *PIA2_REG1;
    if (val != prev) {
      printf("%02x\n", val);
      (void) *PIA2_REG0; // Clear the CD flag
    }
  }
#endif

  return;
}
#endif /* OBSOLETE */

#define MIDI_MAX 127

#define OSC_RELEASE 0
#define OSC_PRESS 1
#define OSC_UNUSABLE 2

static uint8_t started_playback;
static uint8_t started_graphics;
static uint8_t opened_socket;

/* Everything fatal funnels through here. Unwind in order, hand back the text
   screen, and only then print: printf on a live graphics screen flips the VDG
   to text and wipes the display, so a message printed first would land on a
   half-destroyed picture. */
static void fatal(const char *what, uint16_t code)
{
  uint8_t was_graphics = started_graphics;

  /* Order matters. Leave graphics mode and unwind everything first, THEN
     clear, THEN print. printf on a live graphics screen flips the VDG to text
     and leaves the message sitting on wreckage; clearing before the unwind
     lets whatever the unwind disturbs land on top of the message; and clearing
     after printing would wipe the message itself.

     Clear ONLY if there was a graphics screen to get off. In text mode there
     is nothing to escape and everything to keep -- an unconditional clear here
     wiped the status lines and the error with them, so a failure looked like a
     silent exit to an OK prompt. */
  if (started_graphics) {
    end_graphics();
    started_graphics = 0;
  }

  if (started_playback) {
    stop_playback();
    serial_on();
  }
  if (opened_socket)
    network_close(SOCKET);

  if (was_graphics)
    cls(1);

  printf("\n%s: %u\n", what, code);

  /* Hold it. Returning straight to BASIC took the message with it, and the
     failure was indistinguishable from a silent exit.

     Drain the keyboard before waiting. The keystroke that answered the demo
     prompt can still be repeating, and a stale key satisfied this wait the
     instant it was reached -- the message was printed, dismissed and gone in
     less time than it takes to see, which looked exactly like a silent exit
     to an OK prompt. */
  while (inkey())
    ;
  printf("\nPRESS ANY KEY\n");
  waitkey(0);

  exit(1);
}

/* Press or release, read from the OSC type tag rather than assumed.

   The controller's layout author picks the argument type per message -- in
   TouchOSC it can be FLOAT, INTEGER or BOOLEAN -- so all three have to work.
   Testing only the first argument byte, as this used to, is right for a float
   and wrong for the others: a small integer's first byte is zero, so every
   press read as a release and nothing ever played; and a boolean carries NO
   argument bytes at all, so the closing frame marker got read as the value,
   which is always non-zero, so notes played and never stopped. */
static uint8_t osc_note_state(char *types, uint8_t *values, uint16_t avail)
{
  uint8_t i;

  switch (types[1]) {
  case 'T':
    return OSC_PRESS;

  case 'F':
  case 'N':
    return OSC_RELEASE;

  case 'f':
  case 'i':
    if (avail < 4)
      return OSC_UNUSABLE;

    /* Any non-zero magnitude is a press. The float sign bit is masked off so
       -0.0 releases the same as 0.0. */
    if (values[0] & (types[1] == 'f' ? 0x7F : 0xFF))
      return OSC_PRESS;
    for (i = 1; i < 4; i++)
      if (values[i])
        return OSC_PRESS;
    return OSC_RELEASE;

  default:
    return OSC_UNUSABLE;
  }
}

/* Length through the closing SLIP delimiter of the first complete frame, or 0
   while it is still arriving. Leading junk and runs of delimiters are folded
   in so the caller consumes them -- back-to-back 0xC0 is ordinary SLIP, not an
   error, and warning about it would cry wolf on every packet. */
static uint16_t frame_length(uint8_t *buf, uint16_t len)
{
  uint16_t a, b;

  for (a = 0; a < len && buf[a] != FRAMING; a++)
    ;
  if (a >= len)
    return 0;

  while (a + 1 < len && buf[a + 1] == FRAMING)
    a++;

  for (b = a + 1; b < len && buf[b] != FRAMING; b++)
    ;
  if (b >= len)
    return 0;

  return b + 1;
}

/* Poll passes between connection checks while PROCEED is available. Small
   enough that a departed client is noticed promptly, large enough that the
   brief sound-off each check costs is not heard. */
#define LINK_CHECK_EVERY 600

int main()
{
  int16_t rlen;
  uint16_t buflen, flen, room;
  uint8_t quit, gone;
  uint16_t link_check;
  uint8_t err, status;
  uint16_t avail;
  size_t length;
  char *ptr;
  char *osc_addr, *osc_types, *osc_values;
  bool have_proceed = network_has_proceed();

  init_conductor();

  printf("Demo mode? (Y/N): ");
  {
    int ch = waitkey(0);
    if (ch == 'Y' || ch == 'y') {
      printf("Y\n");
      init_playback();
      started_playback = 1;
      init_graphics();
      started_graphics = 1;
      play_demo();
      waitkey(0);          /* hold the finished screen so it can be looked at */
      end_graphics();
      stop_playback();
      started_playback = 0;
      cls(1);
      exit(0);
    }
    printf("N\n");
  }

#if 0
  init_playback();
  start_note(60);
  for (rlen = 0; rlen < 100; rlen++)
    printf("%d ", rlen);
  printf("\n");
  for (pos = 0; !pos;) {
    for (rlen = 0; rlen < 10; rlen++) {
      if (inkey()) {
        pos = 1;
        break;
      }
      printf("%d ", rlen);
    }
    printf("\n");

    serial_on();

    for (rlen = 0; rlen < 100; rlen++) {
      if (inkey()) {
        pos = 1;
        break;
      }
      printf("%d ", rlen);
    }
    printf("\n");

    sound_on();
  }
  stop_playback();
  serial_on();
  exit(0);
#endif

  if (!fuji_get_adapter_config_extended(&ace))
    strcpy(ace.fn_version, "FAIL");

  printf("FujiNet: %-14s\n", ace.fn_version);

#if 0
  printf("Loading 4VOICE.BIN...\n");
  err = readDECBFile(ORG_4VOICE, 0, "4VOICE  BIN", buffer, &length);
  if (err) {
    printf("FAILED TO LOAD: %d\n", err);
    exit(1);
  }
  printf("Loaded\n");
#endif

  printf("Opening OSC listener %s:%s...\n", ace.hostname, PORT);
  err = network_open(SOCKET, OPEN_MODE_RW, 0);
  if (err)
    fatal("LISTEN FAILED", err);
  opened_socket = 1;

  printf("Waiting for connection\n");

  /* Up before the connection loop and left up between connections: tearing the
     display down and rebuilding it on every reconnect would flash the screen,
     and printf here would drop the VDG back to text. */
  init_playback();
  started_playback = 1;
  init_graphics();
  started_graphics = 1;
#ifdef OBSOLETE
  setup_cd_flag();
#endif /* OBSOLETE */

  (void) *PIA2_REG0; // Clear the CD flag

  /* One pass per client. A controller that drops -- an app backgrounded, a
     phone off wifi -- used to end the program, because accept happened once. */
  for (;;) {
    graphics_link(0);
    quit = 0;

    for (;;) {
      if (inkey()) {
        quit = 1;
        break;
      }
      serial_on();
      err = network_status(SOCKET, &avail, &status, &err);
      sound_on();
      if (err)
        fatal("STATUS FAILED", err);
      if (status == 1)
        break;
    }

    if (quit)
      break;

    serial_on();
    err = network_accept(SOCKET);
    sound_on();
    if (err)
      fatal("ACCEPT FAILED", err);

    graphics_link(1);
    buflen = 0;
    gone = 0;
    link_check = 0;

    for (;;) {
      avail = 0;
      err = NETWORK_SUCCESS;
      quit = 0;

      for (;;) {
        graphics_warn_tick();

        if (inkey()) {
          quit = 1;
          break;
        }

        // Check if data is available
#ifdef OBSOLETE
        if ((*PIA2_REG1) & 0x80) {
          while ((*PIA2_REG1) & 0x80)
            (void) *PIA2_REG0; // Clear the CD flag
          avail = sizeof(buffer);
          break;
        }
#else
        switch (have_proceed) {
        case true:
          if (network_proceed())
            avail = sizeof(buffer);
          /* PROCEED announces data, never a peer that has simply closed, so
             asking is the only way to notice the client has gone -- and
             without it the loop waits for data that is never coming, which is
             what made a reconnect impossible. Asking every pass would chop the
             audio, since serial_on() kills sound, so ask occasionally. */
          else if (++link_check >= LINK_CHECK_EVERY) {
            link_check = 0;
            serial_on();
            network_status(SOCKET, &avail, &status, &err);
            sound_on();
            if (!status && !avail)
              gone = 1;
          }
          break;
        default:
          serial_on();
          network_status(SOCKET, &avail, &status, &err);
          sound_on();
          if (!status && !avail)
            gone = 1;
          break;
        }

        if (avail || gone || err != NETWORK_SUCCESS)
          break;
#endif /* OBSOLETE */
      }

      //printf("STATUS: A=%u S=%u E=%u\n", avail, status, err);

      if (quit)
        break;

      if (gone)
        break;            /* client vanished without closing cleanly */

      if (err != NETWORK_SUCCESS)
        fatal("CONNECTION ERROR", err);

      /* Reads append to whatever is already held. A packet split across two
         reads used to have its tail thrown away, silently losing a note -- and a
         lost note-OFF leaves that note sounding and its key lit for good. */
      room = (uint16_t) sizeof(buffer) - buflen;
      if (!room) {
        /* Full, with no complete frame in it: the stream is not SLIP. */
        buflen = 0;
        graphics_warn(WARN_OVERRUN);
        continue;
      }
      if (avail > room)
        avail = room;

      serial_on();
      rlen = network_read_nb(SOCKET, buffer + buflen, avail);
      sound_on();

      if (rlen <= 0)
        break;            /* peer closed: an ordinary end, not a failure */

      buflen += (uint16_t) rlen;

      for (;;) {
        uint16_t note;
        uint8_t state;

        flen = frame_length(buffer, buflen);
        if (!flen)
          break;          /* frame still arriving -- keep it for the next read */

        parse_osc_message(buffer, flen, &osc_addr, &osc_types, &osc_values);

        if (!osc_addr)
          graphics_warn(WARN_BAD_PACKET);
        else {
          note = (uint16_t) atoi(osc_addr + 1);
          state = osc_note_state(osc_types, (uint8_t *) osc_values,
                                 flen - (uint16_t) ((uint8_t *) osc_values
                                                    - buffer));
          if (note > MIDI_MAX)
            graphics_warn(WARN_BAD_NOTE);
          else if (state == OSC_UNUSABLE)
            graphics_warn(WARN_BAD_TYPE);
          else if (state == OSC_PRESS)
            start_note(note);
          else
            stop_note(note);
        }

        buflen -= flen;
        if (buflen)
          memmove(buffer, buffer + flen, buflen);
      }
    }

    if (quit)
      break;

    /* The client is gone. Its note-OFFs are never arriving, so release
       everything before listening again -- otherwise a note left sounding
       stays sounding, with its key lit, across the next connection. Drop only
       the client: the listener stays open so a reconnect just works. */
    stop_all_notes();
    serial_on();
    network_close_client(SOCKET);
    sound_on();
  }

  if (started_graphics)
    end_graphics();

  stop_playback();
  started_playback = 0;
  serial_on();
  network_close(SOCKET);

  if (started_graphics) {
    cls(1);
    started_graphics = 0;
  }

  /* Never exit without saying why. The connection loop is left only by a
     keypress now -- a dropped client is handled inside it -- so this is the
     only way the program ends short of fatal(). */
  printf("\nSTOPPED\n");

  // FIXME - don't leave interupts off?

  return 0;
}
