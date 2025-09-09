EXECUTABLE = $(R2R_PD)/$(PRODUCT).prg
DISK = $(R2R_PD)/$(PRODUCT).dsk

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/compilers/cc65.mk

r2r:: $(EXECUTABLE)
