EXECUTABLE = $(R2R_PD)/$(PRODUCT).bin
DISK = $(R2R_PD)/$(PRODUCT).dsk

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/compilers/cmoc.mk

DISK_EXTRA := $(DISK_EXTRA_$(PLATFORM_UC))
EXECUTABLE_EXTRA := $(EXECUTABLE_EXTRA_$(PLATFORM_UC))

r2r:: $(DISK)
	@make -f $(PLATFORM_MK) $(PLATFORM)/r2r-post

$(DISK): $(EXECUTABLE) $(DISK_EXTRA) | $(R2R_PD)
	$(RM) $@
	decb dskini $@
	decb copy -b -2 $< $@,$(shell echo $(PRODUCT) | tr '[:lower:]' '[:upper:]').BIN
	@make -f $(PLATFORM_MK) $(PLATFORM)/disk-post
