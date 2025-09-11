define pop
  $(wordlist 1, $(shell echo $$(($(words $(1)) - 1))), $(1))
endef

# Automatically figure out PLATFORM from the .mk file that included us
MKLIST_PREV = $(call pop,$(MAKEFILE_LIST))
PLATFORM := $(basename $(notdir $(lastword $(MKLIST_PREV))))
$(info Building for PLATFORM=$(PLATFORM))

include $(MWD)/../Makefile

R2R_PD := $(R2R_DIR)/$(PLATFORM)

CACHE_PLATFORM := $(CACHE_DIR)/$(PLATFORM)

# Find all the CFILES and AFILES
CFILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
AFILES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.s))
$(info CFILES=$(CFILES))

OBJ_DIR := $(BUILD_DIR)/$(PLATFORM)
OBJS := $(addprefix $(OBJ_DIR)/, $(notdir $(CFILES:.c=.o) $(AFILES:.s=.o)))
$(info OBJS=$(OBJS))

MKDIR_P ?= mkdir -p

$(info EXECUTABLE=$(EXECUTABLE))
$(EXECUTABLE): $(OBJS) | $(R2R_PD)
	$(link-bin)

$(OBJ_DIR) $(R2R_PD) $(CACHE_PLATFORM):
	$(MKDIR_P) $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(compile)
$(OBJ_DIR)/%.o: %.s | $(OBJ_DIR)
	$(compile)

vpath %.c $(SRC_DIRS)
vpath %.s $(SRC_DIRS)

clean::
	rm -rf $(OBJ_DIR) $(CACHE_PLATFORM) $(R2R_PD)

debug::
	echo 'What should debug target do?'
	exit 1

# include autodeps
-include $(wildcard $(OBJ_DIR)/*.d)

# Fill in the FUINET_LIB_* variables by calling fnlib.py. It's a bit
# messy because of workarounds needed for dealing with newlines and
# $(eval)
define _newline


endef
$(eval $(subst |,$(_newline),$(shell PLATFORM=$(PLATFORM) CACHE_DIR=$(CACHE_DIR) $(MWD)/fnlib.py $(FUJINET_LIB) | tr '\n' '|')))
ifeq ($(strip $(FUJINET_LIB_LDLIB)),)
  $(error fujinet-lib not available)
endif
