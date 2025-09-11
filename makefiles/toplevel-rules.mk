# Executables and disk images will be placed into a platform specific
# subdirectory in a "Ready 2 Run" directory (r2r)
R2R_DIR = r2r
BUILD_DIR = build
CACHE_DIR = _cache

MAKEFILE_DIR = makefiles

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
APP_TARGETS := $(foreach p, $(PLATFORMS), $(R2R_DIR)/$(p)/$(PRODUCT))

.PHONY: all clean FORCE

all:: $(APP_TARGETS)

clean::
	rm -rf $(R2R_DIR) $(BUILD_DIR) $(CACHE_DIR)

# Use % wildcard match to platform specific app so we don't have to
# spell out every single platform variation
$(R2R_DIR)/%/$(PRODUCT): FORCE
	$(MAKE) -f $(MAKEFILE_DIR)/platforms/$*.mk r2r

# Convenience: allow `make coco` (or apple2) as a shortcut
$(PLATFORMS): %: $(R2R_DIR)/%/$(PRODUCT)

# ------------------------------------------------------------------------
# Pattern rule to support "make <platform>/<target>" syntax.
#
# Example: "make apple2/r2r"
#   $@   = "apple2/r2r"         (the full target name)
#   $(@D) = "apple2"             (the directory part before the slash)
#   $(@F) = "r2r"                (the filename part after the slash)
#
# This runs the corresponding platform makefile:
#   make -f makefiles/platforms/apple2.mk r2r
#
# Works for ANY target name, so:
#   make coco/clean   -> runs clean in makefiles/platforms/coco.mk
#   make atari/debug  -> runs debug in makefiles/platforms/atari.mk
# ------------------------------------------------------------------------
.DEFAULT:
	@target="$@" ; case "$@" in \
	  */*/*)   echo "No rule to make target '$@'"; exit 1;; \
	  */*)     platform=$${target%/*}; target=$${target##*/}; \
	           $(MAKE) -f makefiles/platforms/$${platform}.mk $${target} ;; \
	  *)       echo "No rule to make target '$@'"; exit 1;; \
	esac
