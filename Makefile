PRODUCT = jammin8
PLATFORMS = coco
#PLATFORMS += apple2 atari c64 adam msdos

# You can run 'make <platform>' to build for a specific platform,
# or 'make <platform>/<target>' for a platform-specific target.
# Example shortcuts:
#   make coco        → build for coco
#   make apple2/disk → build the 'disk' target for apple2

# SRC_DIRS may use the literal %PLATFORM% token.
# It expands to the chosen PLATFORM plus any of its combos.
SRC_DIRS = src src/%PLATFORM%

# FUJINET_LIB can be
# - a version number such as 4.7.6
# - a directory which contains the libs for each platform
# - a zip file with an archived fujinet-lib
# - a URL to a git repo
# - empty which will use whatever is the latest
# - undefined, no fujinet-lib will be used
FUJINET_LIB = https://github.com/FozzTexx/fujinet-lib-experimental.git

# Define extra dirs ("combos") that expand with a platform.
# Format: platform+=combo1,combo2
PLATFORM_COMBOS = \
  c64+=commodore \
  atarixe+=atari

include makefiles/toplevel-rules.mk

# If you need to add extra platform-specific steps, do it here:
#   coco/r2r:: coco/custom-step1
#   coco/r2r:: coco/custom-step2
# or
#   apple2/disk: apple2/custom-step1 apple2/custom-step2

# DISK_POSTDEPS_COCO := r2r/coco/4voice.bin
# coco/disk-post::
# 	for FILE in $(DISK_POSTDEPS_COCO) ; do \
# 	    DEST="$$(basename $${FILE} | tr '[:lower:]' '[:upper:]')" ; \
# 	    decb copy -b -2 "$${FILE}" "$(DISK),$${DEST}" ; \
# 	done

4VOICE_BIN = r2r/coco/4voice.bin
EXECUTABLE_POSTDEPS_COCO := $(4VOICE_BIN)
coco/executable-post:: $(4VOICE_BIN)
	./merge-coco-bin.py $(4VOICE_BIN) $(EXECUTABLE) $(EXECUTABLE)

$(4VOICE_BIN):: src/coco/4voice/4voice.s | $(R2R_PD)
	lwasm -b -9 -o $@ $<
