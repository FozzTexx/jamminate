#include "coco/4voice.h"
#include "coco/serial.h"
#include "conductor.h"

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
  size_t length;
  char *ptr;
  char *osc_addr, *osc_types, *osc_values;


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

  printf("Searching for FujiNet...\n");
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
  printf("Waiting for connection\n");

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

  printf("Connected\n");

  init_playback();
  setup_cd_flag();

  (void) *PIA2_REG0; // Clear the CD flag
  printf("Waiting for data...\n");
  for (;;) {
    for (;;) {
      if (inkey()) {
        avail = 0;
        err = -1;
        break;
      }

      // Check if data is available
      if ((*PIA2_REG1) & 0x80) {
        while ((*PIA2_REG1) & 0x80)
          (void) *PIA2_REG0; // Clear the CD flag
        avail = sizeof(buffer);
        break;
      }
    }

    //printf("STATUS: A=%u S=%u E=%u\n", avail, status, err);

    if (err) {
      printf("CONNECTION ERROR: A=%u S=%u E=%u\n", avail, status, err);
      break;
    }

    serial_on();
    rlen = network_read_nb(SOCKET, buffer, avail);
    sound_on();

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
      uint16_t note;


      end_of_packet = parse_osc_message(&buffer[pos], rlen,
                                        &osc_addr, &osc_types, &osc_values);
      if (!osc_addr)
        break;

      //printf("Addr: %s  Types: %s  Value: %d\n", osc_addr, osc_types, *osc_values);

      note = atoi(osc_addr + 1);
      if (*osc_values)
        start_note(note);
      else
        stop_note(note);

      rlen -= end_of_packet;
      pos += end_of_packet;
    }

    if (rlen) {
      printf("Received: %d\n", rlen);
      //hexdump(&buffer[pos], rlen);
    }
  }

  stop_playback();
  serial_on();
  network_close(SOCKET);

  // FIXME - don't leave interupts off?

  return 0;
}
