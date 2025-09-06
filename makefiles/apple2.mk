PLATFORM=apple2
CC=cl65
# Converts AppleSingle (cc65 output) to AppleDouble (netatalk share)
UNSINGLE=unsingle

-include defs.mk

define single-to-double
  unsingle $< && mv $<.ad $@ && mv .AppleDouble/$<.ad .AppleDouble/$@
endef

all: $(TARGET).$(PLATFORM) $(TARGET).po

$(TARGET).$(PLATFORM): $(TARGET).a2s
	if command -v $(UNSINGLE) > /dev/null 2>&1 ; then \
	  $(single-to-double) ; \
	else \
	  cp $< $@ ; \
	fi

$(TARGET).a2s: $(OBJS)
	echo OBJS: $(OBJS)
	$(link-bin)

$(OBJDIR)/main.o: $(FNLIB_LIBS)/fujinet.$(PLATFORM).lib

$(TARGET).po: $(TARGET).a2s
	ac -pro140 $@ $(TARGET)
	cat $< | ac -as $@ $(TARGET)

-include cc65.common.mk
-include post.mk
