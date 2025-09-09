EXECUTABLE = $(R2R_PD)/$(PRODUCT).bin
DISK = $(R2R_PD)/$(PRODUCT).dsk

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/compilers/cmoc.mk

r2r:: $(DISK)

$(DISK): $(EXECUTABLE) | $(R2R_PD)
	$(RM) $@
	decb dskini $@
	decb copy -b -2 $< $@,$(shell echo $(PRODUCT) | tr '[:lower:]' '[:upper:]').BIN
