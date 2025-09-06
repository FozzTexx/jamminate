TARGET = $(R2R_PD)/$(APP).a2s
DISK = $(R2R_PD)/$(APP).po

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/compilers/cc65.mk

r2r:: $(DISK)

$(DISK): $(TARGET) | $(R2R_PD)
	ac -pro140 $@ $(APP)
	cat $< | ac -as $@ $(APP)
# FIXME - add PRODOS
# FIXME - add $(APP).SYSTEM

# Converts AppleSingle (cc65 output) to AppleDouble (netatalk share)
UNSINGLE = unsingle
TARGET_AD = $(R2R_PD)/$(APP)

define single-to-double
  unsingle $< && mv $<.ad $@ && mv .AppleDouble/$<.ad .AppleDouble/$@
endef

$(TARGET_AD): $(TARGET)
	if command -v $(UNSINGLE) > /dev/null 2>&1 ; then \
	  $(single-to-double) ; \
	else \
	  cp $< $@ ; \
	fi
