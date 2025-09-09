#include <fujinet-fuji.h>
#include <fujinet-network.h>
#include <fujinet-network-coco.h>

#define PORT "6573"
#define SOCKET "N:TCP://:" PORT

AdapterConfigExtended ace;

uint8_t buffer[256];

#define COLUMNS 8

void hexdump(uint8_t *buffer, int count)
{
  int outer, inner;
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

int main()
{
  int16_t rlen;
  uint8_t err, status;
  uint16_t avail;


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
#if 0
    for (;;) {
      // Wait for data to become available
      if (network_status(SOCKET, &avail, &status, &err))
        break;
      if (avail)
        break;
    }
#endif

    rlen = network_read_nb(SOCKET, buffer, sizeof(buffer) - 1);
    if (!rlen)
      continue;
    if (rlen < 0)
      break;

    printf("Received: %d\n", rlen);
    hexdump(buffer, rlen);
  }

  return 0;
}
