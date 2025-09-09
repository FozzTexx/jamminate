EXECUTABLE = $(R2R_PD)/$(PRODUCT).exe

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/compilers/ow2.mk

CFLAGS += -D__MSDOS__

r2r:: $(EXECUTABLE)
