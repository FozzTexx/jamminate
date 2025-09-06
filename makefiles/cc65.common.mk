CC=cl65
CFLAGS=-O --cpu 6502 $(FNLIB_INCLUDE)
AFLAGS=--cpu 6502
LIBS=-L $(FNLIB_LIBS) fujinet.$(PLATFORM).lib

define link-bin
  $(CC) -vm -t $(PLATFORM) $(LDFLAGS) $^ $(LIBS) -o $@
endef

define compile
  $(CC) -l $(basename $@).lst -c $(CFLAGS) -t $(PLATFORM) -o $@ $<
endef

define assemble
  $(CC) -l $(basename $@).lst -c $(AFLAGS) -t $(PLATFORM) -o $@ $<
endef
