# Check zip time
# git URL

PRODUCT = jammin8
PLATFORMS = coco
#PLATFORMS += apple2 atari c64 adam msdos

SRC_DIRS = src src/$(PLATFORM)

# FUJINET_LIB can be
# - a version number such as 4.7.6
# - a directory which contains the libs for each platform
# - a zip file with an archived fujinet-lib
# - empty which will use whatever is the latest
FUJINET_LIB = ../fujinet-lib/build

EXTRA_INCLUDE = ../fujinet-lib/coco/src/include
#EXTRA_INCLUDE = ../fujinet-lib-unified/bus/coco

# FIXME - Hack to build inside of defoogi. Maybe only fallback on
#         defoogi if build tools are missing?
#MAKE := defoogi -e FUJINET_LIB $(MAKE)

include makefiles/toplevel-rules.mk
