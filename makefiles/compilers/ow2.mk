CC = wcc
AS = wasm
CFLAGS = -0 -bt=dos -ms -s -osh -zu -I$(FUJINET_LIB_INCLUDE)
AFLAGS =
LIBS = $(FUJINET_LIB_LDLIB)
LD = wlink OPTION quiet
LDFLAGS = SYSTEM dos OPTION MAP LIBPATH $(FUJINET_LIB_DIR)

define link-bin
  $(LD) $(LDFLAGS) \
    disable 1014 \
    name $@ \
    file {$^} \
    library {$(LIBS)}
endef

define compile
  $(CC) $(CFLAGS) -ad=$(OBJ_DIR)/$(basename $(notdir $<)).d -fo=$@ $<
endef

define assemble
  $(CC) -c $(AFLAGS) -o $@ $< 2>&1
endef
