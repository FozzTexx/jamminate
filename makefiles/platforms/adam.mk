EXECUTABLE = $(R2R_PD)/$(PRODUCT).prg

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/compilers/z88dk.mk

r2r:: $(EXECUTABLE)
