CC = zcc
AS = z80asm
CFLAGS += +coleco -subtype=adam
CFLAGS += $(foreach incdir,$(EXTRA_INCLUDE),-I$(incdir))
AFLAGS =
LDFLAGS += +coleco -subtype=adam

ifdef FUJINET_LIB_INCLUDE
  CFLAGS += -I$(FUJINET_LIB_INCLUDE)
endif
ifdef FUJINET_LIB_DIR
  LIBS += -L$(FUJINET_LIB_DIR) -l$(FUJINET_LIB_LDLIB)
endif

define link-bin
  $(CC) $(LDFLAGS) $2 $(LIBS) -o $1
endef

define compile
  $(CC) -c $(CFLAGS) -o $1 $2
endef

define assemble
  $(CC) -c $(AFLAGS) -o $1 $2
endef
