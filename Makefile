APP = jammin8
PLATFORMS = coco apple2

# FUJINET_LIB can be
# - a version number such as 4.7.4
# - a directory which contains the libs for each platform
# - a zip file with an archived fujinet-lib
# - empty which will use whatever is the latest
FUJINET_LIB = 4.7.4

# FIXME - Hack to build inside of defoogi. Maybe only fallback on
#         defoogi if build tools are missing?
#MAKE := defoogi -e FUJINET_LIB $(MAKE)

# Executables and disk images will be placed into a platform specific
# subdirectory in a "Ready 2 Run" directory (r2r)
R2R_DIR = r2r

# This Makefile is done in a way to be able to build an application
# which can run on multiple platforms.
#
# My main design goals:
#
# 1. Allow make to handle dependencies directly and execute only
#    necessary commands so that `make <target>` works
# 2. Modular platform makefiles that define functions so
#    compile/assembly/link is handled the same way on each platform
# 3. The main things a developer would need to modify (such as
#    variables with source file lists) are in this Makefile
# 4. Be able to directly call a <platform>.mk with `make -f`#
# 5. Short simple makefiles that aren't filled with all kinds of
#    ifdefs
#
# Unfortunately I have failed on simplicity. It's still yucky
# complicated stuff that I hate because nobody else can make sense of
# it.

# Make a list of the things we want to build which combine R2R dir, app name, and platform
APP_TARGETS := $(foreach p, $(PLATFORMS), $(R2R_DIR)/$(p)/$(APP))

$(info APP_TARGETS=$(APP_TARGETS))
all:: $(APP_TARGETS)

export FUJINET_LIB

# Use % wildcard match to platform specific app so we don't have to
# spell out every single platform variation
$(R2R_DIR)/%/$(APP): FORCE
	$(MAKE) -f $(MAKEFILE_DIR)/platforms/$*.mk r2r

.PHONY: all FORCE

# Convenience: allow `make coco` (or apple2) as a shortcut
$(PLATFORMS): %: $(R2R_DIR)/%/$(APP)

MAKEFILE_DIR = makefiles
