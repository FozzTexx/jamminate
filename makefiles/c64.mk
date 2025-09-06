PLATFORM=c64
-include defs.mk

all: $(TARGET).$(PLATFORM)

$(TARGET).$(PLATFORM): $(OBJS)
	$(link-bin)

$(OBJDIR)/main.o: $(FNLIB_LIBS)/fujinet.$(PLATFORM).lib

-include cc65.common.mk
-include post.mk
