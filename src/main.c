#include "fujinet-fuji.h"

#ifndef _CMOC_VERSION_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

AdapterConfigExtended ace;

int main()
{
  printf("Searching for FujiNet...\n");
  if (!fuji_get_adapter_config_extended(&ace))
    strcpy(ace.fn_version, "FAIL");

  printf("FujiNet: %-14s  Make: ???\n", ace.fn_version);

  return 0;
}
