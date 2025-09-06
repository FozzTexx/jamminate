define pop
  $(wordlist 1, $(shell echo $$(($(words $(1)) - 1))), $(1))
endef

MKLIST_PREV = $(call pop,$(MAKEFILE_LIST))
PLATFORM := $(basename $(notdir $(lastword $(MKLIST_PREV))))
$(info Building for PLATFORM=$(PLATFORM))

include $(MWD)/defs.mk
include $(MWD)/fnlib.mk
