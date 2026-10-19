# Level Devil — run to the door; the level has other plans.
#   make          build output/leveldevil.nwa
#   make check    link the app like the calculator does (output/leveldevil.bin)
#   make test     every room played through on a computer with its solution (tests/play.c,
#                 tests/solutions.txt)
#
# The rooms are written in tools/rooms.py, which packs them into output/rooms.inc (Python 3).
# In NumPlay the game draws with the launcher's strips, shapes and fonts, saves through its file
# functions and unpacks its rooms with its inflater (module.o leaves them to the launcher); on its
# own it brings them along.
Q ?= @
CC = arm-none-eabi-gcc
NWLINK ?= npx --yes -- nwlink@1.0.0
BUILD_DIR = output
SRC = src/main.c
ROOMS = $(BUILD_DIR)/rooms.inc
HEADERS = ../common/epsilon_app.h ../common/epsilon_files.h ../common/np_text.h ../../launcher/src/gfx.h \
  ../../launcher/src/np.h ../../launcher/src/font.h ../../launcher/src/inflate.h

CFLAGS = -std=c11 $(shell $(NWLINK) eadk-cflags-device) -I$(BUILD_DIR)
CFLAGS += -Os -Wall -Wextra -Wno-unused-parameter -fno-math-errno -fsingle-precision-constant
CFLAGS += -fno-tree-loop-distribute-patterns -flto -fno-fat-lto-objects -fwhole-program -fvisibility=internal
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections -flinker-output=nolto-rel

.PHONY: build check clean host test
build: $(BUILD_DIR)/leveldevil.nwa

check: $(BUILD_DIR)/leveldevil.nwa
	$(Q) $(NWLINK) nwa-bin $< $(BUILD_DIR)/leveldevil.bin
	@echo "BIN     $(BUILD_DIR)/leveldevil.bin: $$(wc -c < $(BUILD_DIR)/leveldevil.bin) bytes"

$(ROOMS): tools/rooms.py | $(BUILD_DIR)
	$(Q) python3 tools/rooms.py $(BUILD_DIR)

# compiled, then linked (the link options would upset the compiler)
$(BUILD_DIR)/main.o: $(SRC) $(ROOMS) $(HEADERS) | $(BUILD_DIR)
	@echo "CC      $<"
	$(Q) $(CC) $(CFLAGS) -c $(SRC) -o $@
$(BUILD_DIR)/gfx.o: ../../launcher/src/gfx.c $(HEADERS) | $(BUILD_DIR)
	$(Q) $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/font_data.o: ../../launcher/src/font_data.c ../../launcher/src/font.h | $(BUILD_DIR)
	$(Q) $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/xfont.o: ../../launcher/src/xfont.c ../../launcher/src/np_xfont.h $(HEADERS) | $(BUILD_DIR)
	$(Q) $(CC) $(CFLAGS) -c $< -o $@
$(BUILD_DIR)/inflate.o: ../../launcher/src/inflate.c ../../launcher/src/inflate.h | $(BUILD_DIR)
	$(Q) $(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/leveldevil.nwa: $(BUILD_DIR)/main.o $(BUILD_DIR)/gfx.o $(BUILD_DIR)/font_data.o $(BUILD_DIR)/xfont.o \
  $(BUILD_DIR)/inflate.o $(BUILD_DIR)/icon.o
	@echo "LD      $@"
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@
	$(Q) arm-none-eabi-strip --strip-unneeded $@

$(BUILD_DIR)/icon.o: src/icon.png | $(BUILD_DIR)
	@echo "ICON    $<"
	$(Q) $(NWLINK) png-icon-o $< $@

$(BUILD_DIR):
	$(Q) mkdir -p $@

clean:
	$(Q) rm -rf $(BUILD_DIR) build

# on a computer, with the sanitizers: the rooms played through, and pictures of them
HOST_FLAGS = -std=gnu11 -O1 -g -Wall -Wextra -Wno-unused-parameter -DPLATFORM_DEVICE=1 -DHOST -I$(BUILD_DIR) \
  $(shell $(NWLINK) eadk-cflags-simulator) -fsanitize=address,undefined -fno-sanitize-recover=undefined
host: build/play
build/play: $(SRC) $(ROOMS) tests/play.c $(HEADERS)
	$(Q) mkdir -p build
	$(Q) cc $(HOST_FLAGS) tests/play.c ../../launcher/src/gfx.c ../../launcher/src/font_data.c \
	  ../../launcher/src/xfont.c ../../launcher/src/inflate.c -lm -o $@
	@echo "HOST    $@"
test: build/play
	build/play solve

# NumPlay launcher: the app as a module (see ../../tools/npmodule.py) for the
# calculator, and as a relocatable object for the simulator build.
.PHONY: module sim-module
module: $(BUILD_DIR)/module.o
$(BUILD_DIR)/module-main.o: $(SRC) $(ROOMS) $(HEADERS) | $(BUILD_DIR)
	$(Q) $(CC) $(CFLAGS) -DNP_MODULE=1 -c $(SRC) -o $@
$(BUILD_DIR)/module.o: $(BUILD_DIR)/module-main.o
	$(Q) $(CC) $(CFLAGS) -Wl,--relocatable -nostartfiles -nostdlib -Wl,-e,main -flinker-output=nolto-rel $< -o $@

sim-module: $(BUILD_DIR)/sim-module.o
$(BUILD_DIR)/sim-module.o: $(SRC) $(ROOMS) $(HEADERS) | $(BUILD_DIR)
	$(Q) mkdir -p $(BUILD_DIR)/sim
	$(Q) cc -std=c11 -O2 -fPIC $(shell $(NWLINK) eadk-cflags-simulator) -I$(BUILD_DIR) -DNP_MODULE=1 \
	  -Dmain=np_leveldevil_main -c $(SRC) -o $(BUILD_DIR)/sim/main.o
	$(Q) ld -r $(BUILD_DIR)/sim/main.o -exported_symbol _np_leveldevil_main -o $@
