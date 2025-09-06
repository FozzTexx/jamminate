CC=cmoc
AS=lwasm
CFLAGS=$(FNLIB_INCLUDE)
AFLAGS=
LIBS=-L $(FNLIB_LIBS) -lfujinet.$(PLATFORM)
SHELL=/bin/bash -o pipefail

define link-bin
  $(CC) -o $@ $(LDFLAGS) $^ $(LIBS) 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define compile
  $(CC) -c $(CFLAGS) --deps=$(<:.c=.$(PLATFORM).d) -o $@ $< 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef

define assemble
  $(CC) -c $(AFLAGS) -o $@ $< 2>&1 | sed -e 's/'$$'\033''[[][0-9][0-9]*m//g'
endef
