CC_DEFAULT ?= cl65
AS_DEFAULT ?= ca65
LD_DEFAULT ?= $(CC_DEFAULT)

include $(MWD)/tc-common.mk

CFLAGS += -O --cpu 6502
AFLAGS += --cpu 6502
LDFLAGS +=

define link-bin
  $(LD) -vm -t $(PLATFORM) $(LDFLAGS) $2 $(LIBS) -o $1
endef

define compile
  $(CC) -l $(basename $1).lst --create-dep $(OBJ_DIR)/$(basename $(notdir $2)).d -c $(CFLAGS) -t $(PLATFORM) -o $1 $2
endef

define assemble
  $(AS) -l $(basename $1).lst -c $(AFLAGS) -t $(PLATFORM) -o $1 $2
endef
