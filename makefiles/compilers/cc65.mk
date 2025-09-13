CC = cl65
AS = ca65
CFLAGS += -O --cpu 6502
CFLAGS += $(foreach incdir,$(EXTRA_INCLUDE),-I $(incdir))
AFLAGS = --cpu 6502

ifdef FUJINET_LIB_INCLUDE
  CFLAGS += -I $(FUJINET_LIB_INCLUDE)
endif
ifdef FUJINET_LIB_DIR
  LIBS = -L $(FUJINET_LIB_DIR) $(FUJINET_LIB_LDLIB)
endif

define link-bin
  $(CC) -vm -t $(PLATFORM) $(LDFLAGS) $^ $(LIBS) -o $@
endef

define compile
  $(CC) -l $(basename $@).lst --create-dep $(OBJ_DIR)/$(basename $(notdir $<)).d -c $(CFLAGS) -t $(PLATFORM) -o $@ $<
endef

define assemble
  $(CC) -l $(basename $@).lst -c $(AFLAGS) -t $(PLATFORM) -o $@ $<
endef
