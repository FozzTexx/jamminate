# Automatically figure out TOOLCHAIN from the .mk file that included us
TOOLCHAIN_MK := $(call pop,$(MAKEFILE_LIST))
TOOLCHAIN := $(basename $(notdir $(lastword $(TOOLCHAIN_MK))))
TOOLCHAIN_UC := $(shell echo "$(TOOLCHAIN)" | tr '[:lower:]' '[:upper:]')
$(info Using TOOLCHAIN=$(TOOLCHAIN))

CC_$(TOOLCHAIN_UC) ?= $(CC_DEFAULT)
AS_$(TOOLCHAIN_UC) ?= $(AS_DEFAULT)
LD_$(TOOLCHAIN_UC) ?= $(LD_DEFAULT)

CC := $(CC_$(TOOLCHAIN_UC))
AS := $(AS_$(TOOLCHAIN_UC))
LD := $(LD_$(TOOLCHAIN_UC))

CFLAGS = $(CFLAGS_EXTRA_$(TOOLCHAIN_UC))
CLFAGS += $(foreach incdir,$(EXTRA_INCLUDE),-I$(incdir))
AFLAGS = $(AFLAGS_EXTRA_$(TOOLCHAIN_UC))
LDFLAGS = $(LDFLAGS_EXTRA_$(TOOLCHAIN_UC))

ifdef FUJINET_LIB_INCLUDE
  CFLAGS += -I$(FUJINET_LIB_INCLUDE)
endif
ifdef FUJINET_LIB_DIR
  LIBS += -L$(FUJINET_LIB_DIR) -l$(FUJINET_LIB_LDLIB)
endif
