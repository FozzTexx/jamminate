CC = zcc
AS = z80asm
CFLAGS = +coleco -subtype=adam -I$(FUJINET_LIB_INCLUDE)
AFLAGS =
LDFLAGS = +coleco -subtype=adam
LIBS = -L$(FUJINET_LIB_DIR) -l$(FUJINET_LIB_LDLIB)

define link-bin
  $(CC) $(LDFLAGS) $^ $(LIBS) -o $@
endef

define compile
  $(CC) -c $(CFLAGS) -o $@ $<
endef

define assemble
  $(CC) -c $(AFLAGS) -o $@ $<
endef
