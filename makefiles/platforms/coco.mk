EXECUTABLE = $(R2R_PD)/$(PRODUCT).bin
DISK = $(R2R_PD)/$(PRODUCT).dsk

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/toolchains/cmoc.mk

r2r:: $(DISK) $(R2R_POSTDEPS)
	@make -f $(PLATFORM_MK) $(PLATFORM)/r2r-post

$(DISK): $(EXECUTABLE) $(DISK_POSTDEPS) | $(R2R_PD)
	$(RM) $@
	decb dskini $@
	decb copy -b -2 $< $@,$(shell echo $(PRODUCT) | tr '[:lower:]' '[:upper:]').BIN
	@make -f $(PLATFORM_MK) $(PLATFORM)/disk-post
