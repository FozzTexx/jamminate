CC = cmoc
AS = lwasm
CFLAGS = -I$(FUJINET_LIB_INCLUDE)
AFLAGS =
LIBS = -L $(FUJINET_LIB_DIR) -l$(FUJINET_LIB_LDLIB)
SHELL = /bin/bash -o pipefail

CFLAGS += $(foreach incdir,$(EXTRA_INCLUDE),-I$(incdir))

define link-bin
  $(CC) -o $@ $(LDFLAGS) $^ $(LIBS) 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define compile
  $(CC) -c $(CFLAGS) --deps=$(OBJ_DIR)/$(basename $(notdir $<)).d -o $@ $< 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define assemble
  $(CC) -c $(AFLAGS) -o $@ $< 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef
