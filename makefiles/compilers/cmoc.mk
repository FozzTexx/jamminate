CC = cmoc
AS = lwasm
CFLAGS = $(CFLAGS_CMOC) --intdir=$(OBJ_DIR) $(foreach incdir,$(EXTRA_INCLUDE),-I$(incdir))
AFLAGS = $(AFLAGS_CMOC)
LDFLAGS = $(LDFLAGS_CMOC)

ifdef FUJINET_LIB_INCLUDE
  CFLAGS += -I$(FUJINET_LIB_INCLUDE)
endif
ifdef FUJINET_LIB_DIR
  LIBS += -L$(FUJINET_LIB_DIR) -l$(FUJINET_LIB_LDLIB)
endif

# Needed because of using sed to strip ANSI color escape sequences
SHELL = /bin/bash -o pipefail

define link-bin
  $(CC) -o $1 $(LDFLAGS) $2 $(LIBS) 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define compile
  $(CC) -c $(CFLAGS) --deps=$(OBJ_DIR)/$(basename $(notdir $2)).d -o $1 $2 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define assemble
  $(CC) -c $(AFLAGS) -o $1 $2 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g ; s/^\(.*\)(\([0-9][0-9]*\)) :/\1:\2:/'
endef
