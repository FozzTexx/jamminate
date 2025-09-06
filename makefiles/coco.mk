MWD := $(dir $(lastword $(MAKEFILE_LIST)))
include $(MWD)/common.mk

DISK := $(R2R_PD)/$(APP).dsk

$(info DISK=$(DISK))

all:: $(DISK)

$(TARGET): $(OBJS)
	$(link-bin)

$(DISK).dsk: $(TARGET) | $(R2R_PD)
	$(RM) $@
	decb dskini $@
	decb copy -t -0 autoexec.bas $@,AUTOEXEC.BAS
	decb copy -b -2 $< $@,$(shell echo $(APP) | tr '[:lower:]' '[:upper:]').BIN

# A little bit of trickery to rebuild if fujinet-lib is updated
$(OBJDIR)/main.o: $(FNLIB_LIBS)/libfujinet.$(PLATFORM).a

include $(MWD)/cmoc.common.mk
include $(MWD)/post.mk
