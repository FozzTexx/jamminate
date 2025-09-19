#define WITH_4VOICE 1
#if 1 // WITH_4VOICE
#include "coco/4voice.h"
#include "coco/serial.h"
#endif

#include <fujinet-fuji.h>
#include <fujinet-network.h>
#include <fujinet-network-coco.h>

#define PORT "6573"
#define SOCKET "N:TCP://:" PORT
#define FRAMING 0xC0

#define PIA2_REG0 ((uint8_t *) 0xFF20)
#define PIA2_REG1 (PIA2_REG0 + 1)

extern void play_string(const char *notes);

AdapterConfigExtended ace;

uint8_t buffer[256];
const char *note_name[] = {
  "C ", "C#", "D ", "D#", "E ", "F ", "F#", "G ", "G#", "A ", "A#", "B ",
};

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

uint8_t network_accept(const char* devicespec)
{
  extern void bus_ready();
  extern void dwwrite(uint8_t *, uint16_t);


  struct _ioctl
  {
    uint8_t opcode;
    uint8_t unit;
    uint8_t cmd;
    uint8_t aux1;
    uint8_t aux2;
  } ioctl;

  ioctl.opcode = OP_NET;
  ioctl.unit = network_unit(devicespec);
  ioctl.cmd = 'A';
  ioctl.aux1 = ioctl.aux2 = 0;

  bus_ready();
  dwwrite((uint8_t *)&ioctl, sizeof(ioctl));

  return network_get_error(ioctl.unit);
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
    printf("SLIP FRAMING NOT FOUND %u %u 0x%02x\n", offset, msglen, msg[offset]);
    return msglen;
  }
  offset++;
  if (msg[offset] != '/') {
    printf("ADDR NOT FOUND\n");
    return msglen;
  }

  addr = &msg[offset];

  // Find null terminator at end of OSC address
  for (pos = 0; offset + pos < msglen && msg[offset + pos]; pos++)
    ;
  if (offset + pos >= msglen || msg[offset + pos]) {
    printf("END OF ADDR NOT FOUND\n");
    return msglen;
  }

  // Skip over padding at end of string, always a multiple of 4 bytes
  pos = ((pos + 3) / 4) * 4;
  if (offset + pos >= msglen || msg[offset + pos] != ',') {
    printf("TYPES NOT FOUND\n");
    return msglen;
  }

  types = &msg[offset + pos];

  // Find null terminator at end of OSC types
  for (; offset + pos < msglen && msg[offset + pos]; pos++)
    ;
  if (offset + pos >= msglen || msg[offset + pos]) {
    printf("END OF TYPES NOT FOUND\n");
    return msglen;
  }

  // Skip over padding at end of string, always a multiple of 4 bytes
  pos = ((pos + 3) / 4) * 4;
  if (offset + pos >= msglen) {
    printf("VALUES NOT FOUND\n");
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

int main()
{
  int16_t rlen, pos;
  uint8_t err, status;
  uint16_t avail;
  char *ptr;
  char *osc_addr, *osc_types, *osc_values;


  // FIXME - init waveform
  // FIXME - setup voice 1
  // FIXME - setup note

#if WITH_4VOICE
  // Start playing
  init_dac();
  init_interrupts();
#endif

  for (avail = 0; avail < 100; avail++)
    printf("%u ", avail);
  printf("\n");

#if 0
  stop_playback();
  serial_on();
  exit(0);
#endif

#if WITH_4VOICE
  serial_on();
#endif
  printf("Searching for FujiNet...\n");
  if (!fuji_get_adapter_config_extended(&ace))
    strcpy(ace.fn_version, "FAIL");

  printf("FujiNet: %-14s  Make: ???\n", ace.fn_version);

  printf("Opening OSC listener %s:%s...\n", ace.hostname, PORT);

  setup_cd_flag();

  err = network_open(SOCKET, OPEN_MODE_RW, 0);
  for (;;) {
    err = network_status(SOCKET, &avail, &status, &err);
    //printf("AVAIL: %u  STATUS: %u  ERR: %u\n", avail, status, err);
    if (err)
      exit(1);
    if (status == 1)
      break;
  }

  printf("Accepting connection...\n");
  err = network_accept(SOCKET);

  if (err) {
    printf("ACCEPT FAIL: %u\n", err);
    exit(1);
  }

#if WITH_4VOICE
  sound_on();
#endif
  (void) *PIA2_REG0; // Clear the CD flag
  printf("Waiting for data...\n");
  for (;;) {
    //printf("WAITING\n");
    for (;;) {
      if (inkey()) {
        avail = 0;
        err = -1;
        break;
      }

      // Check if data is available
#if 0
      if (network_status(SOCKET, &avail, &status, &err))
        break;
      if (avail) {
        err = 0;
        break;
      }
#else
      if ((*PIA2_REG1) & 0x80) {
        while ((*PIA2_REG1) & 0x80)
          (void) *PIA2_REG0; // Clear the CD flag
        avail = sizeof(buffer);
        break;
      }
#endif // 0
    }

    //printf("STATUS: A=%u S=%u E=%u\n", avail, status, err);

    if (err) {
      printf("CONNECTION ERROR: A=%u S=%u E=%u\n", avail, status, err);
      break;
    }

#if WITH_4VOICE
    serial_on();
#endif
    rlen = network_read_nb(SOCKET, buffer, avail);
    //printf("RLEN: %d\n", rlen);
#if WITH_4VOICE
    sound_on();
#endif
#if 0
    if (!rlen)
      continue;
#endif
    if (rlen <= 0) {
      //printf("READ ERROR: %d\n", rlen);
      break;
    }

    for (pos = 0; rlen;) {
      uint16_t end_of_packet;;


      end_of_packet = parse_osc_message(&buffer[pos], rlen,
                                        &osc_addr, &osc_types, &osc_values);
      if (!osc_addr)
        break;

      {
        uint16_t octave, note, freq;
        uint8_t voice;
        //uint32_t *velocity = (uint32_t *) osc_values;


        //printf("Addr: %s  Types: %s  Value: %d\n", osc_addr, osc_types, *osc_values);

        if (*osc_values) {
          note = atoi(osc_addr + 1);
          octave = note / 12;
          note %= 12;
          //printf("NOTE: %u OCTAVE: %u\n", note, octave);
#if 0 //!WITH_4VOICE
          sprintf((char *) buffer, "O%u;L8;%u;", octave, note + 1);
          printf("PLAYING %s\n", buffer);
          play_string((char *) buffer);
#else
          sprintf((char *) buffer, "%s%u", note_name[note], octave);
          freq = get_freq_value((char *) buffer);
          //printf("ARGS: %04x %04x %04x\n", buffer, scratch, freq);
          //printf("PLAYING -%s- %x\n", buffer, freq);
          for (voice = 1; voice <= 4; voice++)
            start_voice(voice, freq);
          //printf("SCRATCH: %04x\n", scratch);
#endif
        }
        else {
          for (voice = 1; voice <= 4; voice++)
            start_voice(voice, 0); // frequency of 0 "stops" the voice
        }
      }

      rlen -= end_of_packet;
      pos += end_of_packet;
    }

    if (rlen) {
      printf("Received: %d\n", rlen);
      //hexdump(&buffer[pos], rlen);
    }
  }

#if WITH_4VOICE
  stop_playback();
  serial_on();
  // FIXME - don't leave interupts off
#endif

  network_close(SOCKET);

  return 0;
}
