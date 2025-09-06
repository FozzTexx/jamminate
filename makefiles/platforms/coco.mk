TARGET = $(R2R_PD)/$(APP).bin
DISK = $(R2R_PD)/$(APP).dsk

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/compilers/cmoc.mk

r2r:: $(DISK)

$(DISK): $(TARGET) | $(R2R_PD)
	$(RM) $@
	decb dskini $@
	decb copy -b -2 $< $@,$(shell echo $(APP) | tr '[:lower:]' '[:upper:]').BIN
