CC = cmoc
AS = lwasm
CFLAGS += $(foreach incdir,$(EXTRA_INCLUDE),-I$(incdir))
AFLAGS +=
LDFLAGS +=

ifdef FUJINET_LIB_INCLUDE
  CFLAGS += -I$(FUJINET_LIB_INCLUDE)
endif
ifdef FUJINET_LIB_DIR
  LIBS += -L$(FUJINET_LIB_DIR) -l$(FUJINET_LIB_LDLIB)
endif

# Needed because of using sed to strip ANSI color escape sequences
SHELL = /bin/bash -o pipefail

define link-bin
  $(CC) -o $@ $(LDFLAGS) $^ $(LIBS) 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define compile
  $(CC) -c $(CFLAGS) --deps=$(OBJ_DIR)/$(basename $(notdir $<)).d -o $@ $< 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define assemble
  $(CC) -c $(AFLAGS) -o $@ $< 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g ; s/^\(.*\)(\([0-9][0-9]*\)) :/\1:\2:/'
endef
