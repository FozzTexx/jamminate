TARGET = $(R2R_PD)/$(APP).prg
DISK = $(R2R_PD)/$(APP).dsk

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/compilers/z88dk.mk

r2r:: $(TARGET)
