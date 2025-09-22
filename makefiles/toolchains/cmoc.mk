CC_DEFAULT ?= cmoc
AS_DEFAULT ?= $(CC_DEFAULT)
LD_DEFAULT ?= $(CC_DEFAULT)

include $(MWD)/tc-common.mk

CFLAGS += --intdir=$(OBJ_DIR)
AFLAGS +=
LDFLAGS +=

# Needed because of using sed to strip ANSI color escape sequences
SHELL = /bin/bash -o pipefail

define strip-ansi
  sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define link-bin
  $(LD) -o $1 $(LDFLAGS) $2 $(LIBS) 2>&1 | $(strip-ansi)
endef

define compile
  $(CC) -c $(CFLAGS) --deps=$(OBJ_DIR)/$(basename $(notdir $2)).d -o $1 $2 2>&1 | $(strip-ansi)
endef

define assemble
  $(AS) -c $(AFLAGS) -o $1 $2 2>&1 | $(strip-ansi) | sed -e 's/^\(.*\)(\([0-9][0-9]*\)) :/\1:\2:/'
endef
