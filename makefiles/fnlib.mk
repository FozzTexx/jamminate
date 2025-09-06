# Design goals:
#
# 1. Easily override with local/alternative fujinet-lib
# 2. Allow overriding with a local zip file
# 3. Fetch fujinet-lib if developer hasn't pointed to a dir or zip

# To override where fujinet-lib comes from, set FUJINET_LIB in your
# main Makefile
#
# FUJINET_LIB can be
# - a version number such as 4.7.4
# - a directory which contains the libs for each platform
# - a zip file with an archived fujinet-lib

# Variables that would be useful:
# - FUJINET_LIB_DIR     = path to directory with library archives
# - FUJINET_LIB_ARCHIVE = filename of library
# - FUJINET_LIB_LDFLAGS = linker flags which vary by compiler

$(info FUJINET_LIB=$(FUJINET_LIB))

# FIXME - set FUJINET_LIB_DIR
# FIXME - is FUJINET_LIB a directory? Then use that
# FIXME - not a dir, need to create a cache dir
# FIXME - does cache dir match requested version?
# FIXME - does zip file contain version number?
# FIXME - unzip if zip file
# FIXME - else, download
# FIXME - if no version specified, get latest

ifeq ($(wildcard $(FUJINET_LIB)/.),)
  FUJINET_LIB_DIR := $(FUJINET_LIB)
endif

# FIXME - is this coco? Make sure the library is named correctly or has symlink

# Library naming patterns:

# As built:
#   fujinet.apple2.lib
#   fujinet.atari.lib
#   fujinet.c64.lib
#   fujinet.coco.lib
#   libfujinet.coco.a@

# in zip:
#  fujinet-atari-4.7.7.lib

FUJINET_LIB_GITHUB = https://github.com/FujiNetWIFI/fujinet-lib/releases/download

FUJINET_LIB_ROOT = $(FNLIB_CACHE_DIR)/fujinet-lib
FUJINET_LIB_VERSION_DIR = $(FUJINET_LIB_ROOT)/$(FUJINET_LIB_VERSION)-$(CURRENT_TARGET)
FUJINET_LIB_PATH = $(FUJINET_LIB_VERSION_DIR)/fujinet-$(CURRENT_TARGET)-$(FUJINET_LIB_VERSION).lib

FUJINET_LIB_DOWNLOAD_URL = https://github.com/FujiNetWIFI/fujinet-lib/releases/download/v$(FUJINET_LIB_VERSION)/fujinet-lib-$(CURRENT_TARGET)-$(FUJINET_LIB_VERSION).zip
FUJINET_LIB_DOWNLOAD_FILE = $(FUJINET_LIB_ROOT)/fujinet-lib-$(CURRENT_TARGET)-$(FUJINET_LIB_VERSION).zip

.get_fujinet_lib:
	@if [ ! -f "$(FUJINET_LIB_DOWNLOAD_FILE)" ]; then \
	    if [ -d "$(FUJINET_LIB_VERSION_DIR)" ]; then \
	        echo "A directory already exists with version $(FUJINET_LIB_VERSION) - please remove it first"; \
	    	exit 1; \
	    fi; \
	    HTTPSTATUS=$$(curl -Is $(FUJINET_LIB_DOWNLOAD_URL) | head -n 1 | awk '{print $$2}'); \
	    if [ "$${HTTPSTATUS}" == "404" ]; then \
	    	echo "ERROR: Unable to find file $(FUJINET_LIB_DOWNLOAD_URL)"; \
	    	exit 1; \
	    fi; \
	    echo "Downloading fujinet-lib for $(CURRENT_TARGET) version $(FUJINET_LIB_VERSION) from $(FUJINET_LIB_DOWNLOAD_URL)"; \
	    mkdir -p $(FUJINET_LIB_ROOT); \
	    curl -sL $(FUJINET_LIB_DOWNLOAD_URL) -o $(FUJINET_LIB_DOWNLOAD_FILE); \
	    echo "Unzipping to $(FUJINET_LIB_ROOT)"; \
	    unzip -o $(FUJINET_LIB_DOWNLOAD_FILE) -d $(FUJINET_LIB_VERSION_DIR); \
	    echo "Unzip complete."; \
	fi

CFLAGS += --include-dir $(FUJINET_LIB_VERSION_DIR)
ASFLAGS += --asm-include-dir $(FUJINET_LIB_VERSION_DIR)
LIBS += $(FUJINET_LIB_PATH)
ALL_TASKS += .get_fujinet_lib
