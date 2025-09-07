CC = cl65
AS = ca65
CFLAGS = -O --cpu 6502 -I $(FUJINET_LIB_INCLUDE)
AFLAGS = --cpu 6502
LIBS = -L $(FUJINET_LIB_DIR) $(FUJINET_LIB_LDLIB)

define link-bin
  $(CC) -vm -t $(PLATFORM) $(LDFLAGS) $^ $(LIBS) -o $@
endef

define compile
  $(CC) -l $(basename $@).lst --create-dep $(OBJ_DIR)/$(basename $(notdir $<)).d -c $(CFLAGS) -t $(PLATFORM) -o $@ $<
endef

define assemble
  $(CC) -l $(basename $@).lst -c $(AFLAGS) -t $(PLATFORM) -o $@ $<
endef
