# Turbo Rush on the NumWorks calculator.
#   make            output/turborush.nwa
#   make check      link it like the calculator does (output/turborush.bin)
#   make host       build/play: the game on a computer, for tests and pictures
#   make test       laps, crashes, the rivals finishing every circuit at every level, the Grand Prix and Free Ride,
#                   saves (kept, damaged), random keys, the same race twice (with the sanitizers)
#
# The pictures are in art/ (indexed PNGs); tools/art.py packs them into $(BUILD_DIR)/gen (Python 3 with Pillow and
# numpy).
Q ?= @
CC = arm-none-eabi-gcc
NWLINK ?= npx --yes -- nwlink@1.0.0
PYTHON ?= python3
BUILD_DIR = output
GEN = $(BUILD_DIR)/gen
L = ../../launcher/src

SRC = src/main.c src/track.c src/race.c src/save.c src/view.c src/draw.c src/plat_eadk.c \
      $(L)/gfx.c $(L)/font_data.c $(L)/xfont.c $(GEN)/art.c
HEADERS = $(wildcard src/*.h) $(GEN)/art.h $(L)/gfx.h $(L)/font.h $(L)/np.h ../common/epsilon_app.h \
          ../common/epsilon_files.h
ART = $(wildcard art/*.png art/*/*.png)

CFLAGS = -std=gnu11 $(shell $(NWLINK) eadk-cflags-device) -DPLATFORM_DEVICE=1 -DEPSILON_XL=1 -I$(L) -Isrc -I$(GEN)
CFLAGS += -Os -Wall -Wextra -Wno-unused-parameter -fno-math-errno -fsingle-precision-constant
CFLAGS += -fno-tree-loop-distribute-patterns -flto -fno-fat-lto-objects -fwhole-program -fvisibility=internal
CFLAGS += -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections -flinker-output=nolto-rel

.PHONY: build check clean host test art
build: $(BUILD_DIR)/turborush.nwa

# apps get 133652 bytes of RAM on custom builds of Epsilon 25.2 (148928 or more on official ones)
check: $(BUILD_DIR)/turborush.nwa
	$(Q) $(NWLINK) nwa-bin --ram-length 133652 $< $(BUILD_DIR)/turborush.bin
	@echo "BIN     $(BUILD_DIR)/turborush.bin: $$(wc -c < $(BUILD_DIR)/turborush.bin) bytes"

$(BUILD_DIR)/turborush.nwa: $(SRC) $(HEADERS) $(BUILD_DIR)/icon.o
	@echo "LD      $@"
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $(SRC) $(BUILD_DIR)/icon.o -lm -o $@
	$(Q) arm-none-eabi-strip --strip-unneeded $@

$(BUILD_DIR)/icon.o: src/icon.png | $(BUILD_DIR)
	@echo "ICON    $<"
	$(Q) $(NWLINK) png-icon-o $< $@

$(BUILD_DIR):
	$(Q) mkdir -p $@

$(GEN)/art.c: tools/art.py $(ART)
	@echo "ART     $(GEN)"
	$(Q) $(PYTHON) -I tools/art.py art $(GEN)
$(GEN)/art.h: $(GEN)/art.c
art: $(GEN)/art.c

# on a computer: tests/play.c drives races and the whole app, and saves pictures
HOST_SRC = tests/play.c src/track.c src/race.c src/save.c src/view.c src/draw.c \
           $(L)/gfx.c $(L)/font_data.c $(L)/xfont.c $(GEN)/art.c
HOST_FLAGS = -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -ffp-contract=off -Isrc -I$(L) -I$(GEN) -DHOST
host: build/play
build/play: $(HOST_SRC) src/main.c $(wildcard src/*.h) $(GEN)/art.h
	$(Q) mkdir -p build
	$(Q) cc $(HOST_FLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined $(HOST_SRC) -lm -o $@
	@echo "HOST    $@"
test: build/play
	build/play test build

clean:
	$(Q) rm -rf $(BUILD_DIR) build
