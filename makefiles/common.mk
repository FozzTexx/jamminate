define pop
  $(wordlist 1, $(shell echo $$(($(words $(1)) - 1))), $(1))
endef

# Automatically figure out PLATFORM from the .mk file that included us
MKLIST_PREV := $(call pop,$(MAKEFILE_LIST))
PLATFORM := $(basename $(notdir $(lastword $(MKLIST_PREV))))
$(info Building for PLATFORM=$(PLATFORM))

include $(MWD)/../Makefile

R2R_PD := $(R2R_DIR)/$(PLATFORM)
OBJ_DIR := $(BUILD_DIR)/$(PLATFORM)
CACHE_PLATFORM := $(CACHE_DIR)/$(PLATFORM)
MKDIR_P ?= mkdir -p

# Expand PLATFORM_COMBOS entries into a lookup form
#   c64+=commodore,eightbit -> c64 commodore eightbit
expand_platform_pattern = \
  $(foreach d,$(1), \
    $(if $(findstring %PLATFORM%,$(d)), \
      $(foreach p,$(PLATFORM) $(PLATFORM_COMBOS.$(PLATFORM)), \
        $(subst %PLATFORM%,$(p),$(d))), \
      $(d)))

# The fully expanded list of source directories
SRC_DIRS_EXPANDED := $(call expand_platform_pattern,$(SRC_DIRS))

# Find all the CFILES and AFILES
CFILES := $(foreach dir,$(SRC_DIRS_EXPANDED),$(wildcard $(dir)/*.c))
AFILES := $(foreach dir,$(SRC_DIRS_EXPANDED),$(wildcard $(dir)/*.s)) \
          $(foreach dir,$(SRC_DIRS_EXPANDED),$(wildcard $(dir)/*.asm))

# Need two steps: AFILES may be .s or .asm; `make` swaps one suffix at a time
NORM_AFILES := $(AFILES:.asm=.s)
OBJS := $(addprefix $(OBJ_DIR)/, $(notdir $(CFILES:.c=.o) $(NORM_AFILES:.s=.o)))
$(info OBJS=$(OBJS))

$(info EXECUTABLE=$(EXECUTABLE))
$(EXECUTABLE): $(OBJS) | $(R2R_PD)
	$(link-bin)

# auto-created dirs
AUTO_DIRS := $(OBJ_DIR) $(R2R_PD) $(CACHE_PLATFORM)
$(AUTO_DIRS):
	$(MKDIR_P) $@

$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(compile)
$(OBJ_DIR)/%.o: %.s | $(OBJ_DIR)
	$(assemble)
$(OBJ_DIR)/%.o: %.asm | $(OBJ_DIR)
	$(assemble)

vpath %.c $(SRC_DIRS_EXPANDED)
vpath %.s $(SRC_DIRS_EXPANDED)
vpath %.asm $(SRC_DIRS_EXPANDED)

.PHONY: clean debug r2r $(PLATFORM)/r2r disk $(PLATFORM)/disk

clean::
	rm -rf $(OBJ_DIR) $(CACHE_PLATFORM) $(R2R_PD)

debug::
	echo 'What should debug target do?'
	exit 1

# These targets allow adding platform-specific steps from the top-level Makefile.
# Examples:
#   coco/r2r:: coco/custom-step1
#   coco/r2r:: coco/custom-step2
# or with a single colon:
#   apple2/r2r: apple2/custom-step1 apple2/custom-step2
# The double-colon form appends without overwriting existing deps.
r2r:: $(PLATFORM)/r2r
disk:: $(PLATFORM)/disk

# include autodeps
-include $(wildcard $(OBJ_DIR)/*.d)

ifdef FUJINET_LIB
  # Fill in the FUINET_LIB_* variables by calling fnlib.py. It's a bit
  # messy because of workarounds needed for dealing with newlines and
  # and the $(eval)
  define _newline


  endef
  $(eval $(subst |,$(_newline),$(shell PLATFORM=$(PLATFORM) CACHE_DIR=$(CACHE_DIR) \
      $(MWD)/fnlib.py $(FUJINET_LIB) | tr '\n' '|')))
  ifeq ($(strip $(FUJINET_LIB_LDLIB)),)
    $(error fujinet-lib not available)
  endif
endif # FUJINET_LIB
