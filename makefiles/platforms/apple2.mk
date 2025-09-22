EXECUTABLE = $(R2R_PD)/$(PRODUCT).a2s
DISK = $(R2R_PD)/$(PRODUCT).po

MWD := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))..)
include $(MWD)/common.mk
include $(MWD)/toolchains/cc65.mk

r2r:: $(DISK) $(R2R_POSTDEPS)
	make -f $(PLATFORM_MK) $(PLATFORM)/r2r-post

PRODOS_VERSION = 2.4.3
PRODOS8_DISK := $(CACHE_PLATFORM)/PRODOS8-$(PRODOS_VERSION).po
CC65_UTILS_DIR := $(shell cl65 --print-target-path --target $(PLATFORM))/$(PLATFORM)/util
LOADER_SYSTEM := loader.system

$(DISK): $(EXECUTABLE) $(PRODOS8_DISK) $(DISK_POSTDEPS) | $(R2R_PD)
	acx create -d $@ --format $(PRODOS8_DISK) --prodos --size=140kb --name=$(PRODUCT)
	ac -as $@ $(PRODUCT) < $<
	ac -p $@ $(PRODUCT).SYSTEM SYS 0x2000 < $(CC65_UTILS_DIR)/$(LOADER_SYSTEM)
	make -f $(PLATFORM_MK) $(PLATFORM)/disk-post

# Download and cache ProDOS disk if necessary
PRODOS_URL = https://releases.prodos8.com
PRODOS8_RELEASE := ProDOS_$(subst .,_,$(PRODOS_VERSION)).po
$(PRODOS8_DISK): | $(CACHE_PLATFORM)
	curl --insecure -L -o $@ $(PRODOS_URL)/$(PRODOS8_RELEASE)

# Converts AppleSingle (cc65 output) to AppleDouble (netatalk share)
UNSINGLE = unsingle
EXECUTABLE_AD = $(R2R_PD)/$(PRODUCT)

define single-to-double
  unsingle $< && mv $<.ad $@ && mv .AppleDouble/$<.ad .AppleDouble/$@
endef

$(EXECUTABLE_AD): $(EXECUTABLE)
	if command -v $(UNSINGLE) > /dev/null 2>&1 ; then \
	  $(single-to-double) ; \
	else \
	  cp $< $@ ; \
	fi
