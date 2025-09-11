#include <fujinet-fuji.h>
#include <fujinet-network.h>
#include <fujinet-network-coco.h>

#define PORT "6573"
#define SOCKET "N:TCP://:" PORT

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

void parse_osc_message(uint8_t *msg, uint16_t msglen,
                       char **osc_addr, char **osc_types, char **osc_values)
{
  uint8_t *addr, *types, *values;
  uint16_t pos, offset;


  *osc_addr = *osc_types = *osc_values = NULL;

  // Find first 0xC0
  for (offset = 0; offset < msglen && msg[offset] != 0xC0; offset++)
    ;
  if (offset >= msglen || msg[offset] != 0xC0) {
    printf("SLIP FRAMING NOT FOUND %u %u 0x%02x\n", offset, msglen, msg[offset]);
    return;
  }
  offset++;
  if (msg[offset] != '/') {
    printf("ADDR NOT FOUND\n");
    return;
  }

  addr = &msg[offset];

  // Find null terminator at end of OSC address
  for (pos = 0; offset + pos < msglen && msg[offset + pos]; pos++)
    ;
  if (offset + pos >= msglen || msg[offset + pos]) {
    printf("END OF ADDR NOT FOUND\n");
    return;
  }

  // Skip over padding at end of string, always a multiple of 4 bytes
  pos = ((pos + 3) / 4) * 4;
  if (offset + pos >= msglen || msg[offset + pos] != ',') {
    printf("TYPES NOT FOUND\n");
    return;
  }

  types = &msg[offset + pos];

  // Find null terminator at end of OSC types
  for (pos = 0; offset + pos < msglen && msg[offset + pos]; pos++)
    ;
  if (offset + pos >= msglen || msg[offset + pos]) {
    printf("END OF TYPES NOT FOUND\n");
    return;
  }

  // Skip over padding at end of string, always a multiple of 4 bytes
  pos = ((pos + 3) / 4) * 4;
  if (offset + pos >= msglen) {
    printf("VALUES NOT FOUND\n");
    return;
  }

  values = &msg[offset + pos];

  *osc_addr = (char *) addr;
  *osc_types = (char *) types;
  *osc_values = (char *) values;

  return;
}

int main()
{
  int16_t rlen;
  uint8_t err, status;
  uint16_t avail;
  char *ptr;
  char *osc_addr, *osc_types, *osc_values;


  printf("Searching for FujiNet...\n");
  if (!fuji_get_adapter_config_extended(&ace))
    strcpy(ace.fn_version, "FAIL");

  printf("FujiNet: %-14s  Make: ???\n", ace.fn_version);

  printf("Opening OSC listener %s:%s...\n", ace.localIP, PORT);

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

  printf("Waiting for data...\n");
  for (;;) {
    for (;;) {
      // Wait for data to become available
      if (network_status(SOCKET, &avail, &status, &err))
        break;
      if (avail) {
        err = 0;
        break;
      }
      if (inkey()) {
        err = -1;
        break;
      }
    }

    printf("STATUS: A=%u S=%u E=%u\n", avail, status, err);

    if (err) {
      printf("CONNECTION ERROR: A=%u S=%u E=%u\n", avail, status, err);
      break;
    }

    rlen = network_read_nb(SOCKET, buffer, avail);
#if 0
    if (!rlen)
      continue;
#endif
    if (rlen <= 0) {
      printf("READ ERROR: %d\n", rlen);
      break;
    }

    osc_addr = NULL;
    parse_osc_message(buffer, rlen, &osc_addr, &osc_types, &osc_values);
    if (osc_addr) {
      printf("Addr: %s  Types: %s\n", osc_addr, osc_types);

      // FIXME - decode values

      {
        uint16_t octave, note;


        note = atoi(osc_addr + 1);
        octave = note / 12;
        note %= 12;
        sprintf((char *) buffer, "O%u;%u;", octave, note + 1);
        printf("PLAYING %s\n", buffer);
        play_string((char *) buffer);
      }
    }
    else {
      printf("Received: %d\n", rlen);
      hexdump(buffer, rlen);
    }
  }

  network_close(SOCKET);

  return 0;
}
