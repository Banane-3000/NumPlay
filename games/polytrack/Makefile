# PolyGT 3D on the NumWorks calculator.
#   make            output/polygt3d.nwa
#   make check      link it like the calculator does (output/polygt3d.bin), within the RAM and the app space
#   make host       build/play: the game on a computer, for tests and pictures
#   make test       the roads, laps, physics, crashes, the rivals finishing every circuit at every level, a race
#                   driven to the finish, the endless road, saves (kept, damaged, storage full), random keys and the
#                   same race to the same hash on every computer (with the sanitizers)
Q ?= @
CC = arm-none-eabi-gcc
NWLINK ?= npx --yes -- nwlink@1.0.0
BUILD_DIR = output
L = ../../launcher/src

SRC = src/main.c src/track.c src/race.c src/save.c src/render.c src/draw.c src/plat_eadk.c \
      $(L)/gfx.c $(L)/font_data.c $(L)/xfont.c
HEADERS = $(wildcard src/*.h) $(L)/gfx.h $(L)/font.h $(L)/np.h ../common/epsilon_app.h ../common/epsilon_files.h

CFLAGS = -std=gnu11 $(shell $(NWLINK) eadk-cflags-device) -DPLATFORM_DEVICE=1 -DEPSILON_XL=1 -I$(L) -Isrc
CFLAGS += -Os -Wall -Wextra -Wno-unused-parameter -fno-math-errno -fsingle-precision-constant -ffp-contract=off
CFLAGS += -fno-tree-loop-distribute-patterns -flto -fno-fat-lto-objects -fwhole-program -fvisibility=internal
CFLAGS += -ffunction-sections -fdata-sections
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections -flinker-output=nolto-rel

# apps get 133652 bytes of RAM on custom builds of Epsilon 25.2 (148928 or more on official ones), and the
# calculator's app space is 2031616 bytes
RAM = 133652
APP_SPACE = 2031616

.PHONY: build check clean host test
build: $(BUILD_DIR)/polygt3d.nwa

check: $(BUILD_DIR)/polygt3d.nwa
	$(Q) $(NWLINK) nwa-bin --ram-length $(RAM) $< $(BUILD_DIR)/polygt3d.bin
	$(Q) n=$$(wc -c < $(BUILD_DIR)/polygt3d.bin); [ $$n -le $(APP_SPACE) ] || \
	  { echo "polygt3d.nwa is bigger than the calculator's app space ($(APP_SPACE) bytes)"; exit 1; }; \
	  echo "BIN     $(BUILD_DIR)/polygt3d.bin: $$n bytes, RAM within $(RAM) bytes"

$(BUILD_DIR)/polygt3d.nwa: $(SRC) $(HEADERS) $(BUILD_DIR)/icon.o
	@echo "LD      $@"
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $(SRC) $(BUILD_DIR)/icon.o -lm -o $@
	$(Q) arm-none-eabi-strip --strip-unneeded $@

$(BUILD_DIR)/icon.o: src/icon.png | $(BUILD_DIR)
	@echo "ICON    $<"
	$(Q) $(NWLINK) png-icon-o $< $@

$(BUILD_DIR):
	$(Q) mkdir -p $@

# on a computer: tests/play.c drives races and the whole app, and saves pictures
HOST_SRC = tests/play.c src/track.c src/race.c src/save.c src/render.c src/draw.c $(L)/gfx.c $(L)/font_data.c \
           $(L)/xfont.c
HOST_FLAGS = -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -ffp-contract=off -Isrc -I$(L) -DHOST
host: build/play
build/play: $(HOST_SRC) src/main.c $(wildcard src/*.h)
	$(Q) mkdir -p build
	$(Q) cc $(HOST_FLAGS) -fsanitize=address,undefined -fno-sanitize-recover=undefined $(HOST_SRC) -lm -o $@
	@echo "HOST    $@"
test: build/play
	build/play test build

clean:
	$(Q) rm -rf $(BUILD_DIR) build
