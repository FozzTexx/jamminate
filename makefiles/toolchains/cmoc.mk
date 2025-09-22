CC_DEFAULT ?= cmoc
AS_DEFAULT ?= $(CC_DEFAULT)
LD_DEFAULT ?= $(CC_DEFAULT)

include $(MWD)/tc-common.mk

CFLAGS += --intdir=$(OBJ_DIR)
CLFAGS += $(foreach incdir,$(EXTRA_INCLUDE),-I$(incdir))
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
  $(LD) -o $1 $(LDFLAGS) $2 $(LIBS) 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define compile
  $(CC) -c $(CFLAGS) --deps=$(OBJ_DIR)/$(basename $(notdir $2)).d -o $1 $2 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define assemble
  $(AS) -c $(AFLAGS) -o $1 $2 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g ; s/^\(.*\)(\([0-9][0-9]*\)) :/\1:\2:/'
endef
