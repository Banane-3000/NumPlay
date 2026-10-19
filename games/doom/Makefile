# Doom on the NumWorks calculator: id Software's game, the shareware episode, through RP2040 Doom's engine.
#   make            output/doom.nwa
#   make check      link it like the calculator does (output/doom.bin)
#   make host       build/play: the game on a computer, for tests and screenshots
#   make test       the game played on a computer (tests/check.py, with the sanitizers)
#   make media      the pictures of docs/ and the animations of the repository's docs/media (tools/media.py)
#   make data       src/doom1.whx from the shareware DOOM1.WAD (tools/fetch.py downloads it; WAD=its path)
Q ?= @
# the ARM toolchain on the PATH, or in TOOLCHAIN=<its bin folder>
TOOLCHAIN ?=
BIN = $(if $(TOOLCHAIN),$(TOOLCHAIN)/)
CC = $(BIN)arm-none-eabi-gcc
CXX = $(BIN)arm-none-eabi-g++
NWLINK ?= npx --yes -- nwlink@1.0.0
STRIP ?= $(BIN)arm-none-eabi-strip --strip-unneeded
SIZE = $(BIN)arm-none-eabi-size
BUILD_DIR = output
WAD ?=

# the engine (Chocolate Doom by way of RP2040 Doom), and the calculator's side of it (src/nw)
ENGINE = $(wildcard src/*.c) $(wildcard src/doom/*.c)
NW = src/nw/i_system.c src/nw/i_video.c src/nw/i_input.c src/nw/i_sound.c src/nw/saves.c src/nw/main.c
HEADERS = $(wildcard src/*.h src/doom/*.h src/nw/*.h src/nw/*.inc src/compat/*.h src/compat/*/*.h) ../common/epsilon_app.h \
  ../common/epsilon_files.h
INC = -include src/nw/doomconf.h -Isrc -Isrc/doom -Isrc/compat -Isrc/nw
# (the engine's own code has warnings of its own, left as they are; the calculator's side has none)
ENGINE_W = -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sign-compare \
  -Wno-missing-field-initializers -Wno-unused-parameter -Wno-implicit-fallthrough -Wno-format-truncation \
  -Wno-maybe-uninitialized -Wno-type-limits -Wno-missing-braces -Wno-misleading-indentation \
  -Wno-cast-function-type -Wno-pointer-sign -Wno-char-subscripts -Wno-parentheses -Wno-address -Wno-unused-value \
  -Wno-int-in-bool-context -Wno-array-bounds -Wno-stringop-overflow -Wno-absolute-value -Wno-dangling-pointer

ARCH = $(shell $(NWLINK) eadk-cflags-device)
COMMON = $(ARCH) $(INC) -Os -fno-math-errno -fno-unwind-tables -fno-asynchronous-unwind-tables -fsingle-precision-constant -fno-tree-loop-distribute-patterns \
  -ffunction-sections -fdata-sections -fwrapv -flto -fno-fat-lto-objects -fvisibility=internal -DNDEBUG $(EXTRA)
CFLAGS = -std=gnu11 $(COMMON)
CXXFLAGS = -std=gnu++17 -fno-exceptions -fno-rtti -fno-threadsafe-statics $(COMMON)
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections -flinker-output=nolto-rel $(if $(MAP),-Wl$(comma)-Map$(comma)$(MAP))
comma = ,

OBJ = $(patsubst src/%.c,$(BUILD_DIR)/obj/%.o,$(ENGINE) $(NW) src/nw/plat_eadk.c) $(BUILD_DIR)/obj/pd_render.o

.PHONY: build check clean host test data media
build: $(BUILD_DIR)/doom.nwa

# apps get 133652 bytes of RAM on custom builds of Epsilon 25.2 (148928 or more on official ones)
check: $(BUILD_DIR)/doom.nwa
	$(Q) $(NWLINK) nwa-bin --ram-length 133652 $< $(BUILD_DIR)/doom.bin
	@echo "BIN     $(BUILD_DIR)/doom.bin: $$(wc -c < $(BUILD_DIR)/doom.bin) bytes"

$(BUILD_DIR)/obj/nw/%.o: src/nw/%.c $(HEADERS)
	@mkdir -p $(@D)
	$(Q) $(CC) $(CFLAGS) -Wall -Wextra -Wno-unused-parameter -c $< -o $@
$(BUILD_DIR)/obj/%.o: src/%.c $(HEADERS)
	@mkdir -p $(@D)
	$(Q) $(CC) $(CFLAGS) -Wall -Wextra $(ENGINE_W) -c $< -o $@
$(BUILD_DIR)/obj/pd_render.o: src/pd_render.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(Q) $(CXX) $(CXXFLAGS) -Wall -Wextra $(ENGINE_W) -c $< -o $@
$(BUILD_DIR)/data.o: src/nw/data.S src/doom1.whx | $(BUILD_DIR)
	$(Q) $(CC) -c -DDOOM_WHX='"src/doom1.whx"' $< -o $@

$(BUILD_DIR)/doom.nwa: $(OBJ) $(BUILD_DIR)/data.o $(BUILD_DIR)/icon.o
	@echo "LD      $@"
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $^ -lm -o $@
	$(Q) $(STRIP) $@
	$(Q) $(SIZE) $@

$(BUILD_DIR)/icon.o: src/icon.png | $(BUILD_DIR)
	@echo "ICON    $<"
	$(Q) $(NWLINK) png-icon-o $< $@

$(BUILD_DIR):
	$(Q) mkdir -p $@

# on a computer: tests/play.c plays it with scripted keys and saves screenshots
HOST_FLAGS = -O1 -g -fwrapv -DHOST=1 $(INC) -I../common $(HOST_EXTRA)
HOST_SAN = -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=undefined
HOST_OBJ = $(patsubst src/%.c,build/host/%.o,$(ENGINE) $(NW) src/nw/plat_host.c) build/host/pd_render.o \
  build/host/play.o build/host/data.o
host: build/play
build/host/%.o: src/%.c $(HEADERS)
	@mkdir -p $(@D)
	$(Q) cc -std=gnu11 $(HOST_FLAGS) $(HOST_SAN) -w -c $< -o $@
build/host/pd_render.o: src/pd_render.cpp $(HEADERS)
	@mkdir -p $(@D)
	$(Q) c++ -std=gnu++17 -fno-exceptions -fno-rtti $(HOST_FLAGS) $(HOST_SAN) -w -c $< -o $@
build/host/play.o: tests/play.c $(HEADERS)
	@mkdir -p $(@D)
	$(Q) cc -std=gnu11 $(HOST_FLAGS) $(HOST_SAN) -Wall -Wextra -c $< -o $@
build/host/data.o: src/nw/data.S src/doom1.whx
	@mkdir -p $(@D)
	$(Q) cc -c -DDOOM_WHX='"src/doom1.whx"' $< -o $@
build/play: $(HOST_OBJ)
	$(Q) c++ $(HOST_SAN) $^ -lm -o $@
	@echo "HOST    $@"

test: build/play
	python3 tests/check.py build/play

media: build/play
	python3 tools/media.py build/play

# the packed WAD: whd_gen (RP2040 Doom's converter) built on the computer, without the sounds and music
WHD_GEN = build/whd_gen
WHD_GEN_C = tools/whd_gen/musx_decoder.c tools/whd_gen/adpcm-lib.c src/tiny_huff.c src/image_decoder.c
WHD_GEN_OBJ = $(patsubst %.cpp,build/whd_gen_obj/%.o,$(notdir $(wildcard tools/whd_gen/*.cpp))) \
  $(patsubst %.c,build/whd_gen_obj/%.o,$(notdir $(WHD_GEN_C)))
WHD_GEN_FLAGS = -O2 -DIS_WHD_GEN=1 -Itools/whd_gen -Isrc -Isrc/doom -w
build/whd_gen_obj/%.o: tools/whd_gen/%.cpp $(wildcard tools/whd_gen/*.h)
	@mkdir -p $(@D)
	$(Q) c++ -std=gnu++17 $(WHD_GEN_FLAGS) -c $< -o $@
build/whd_gen_obj/%.o: tools/whd_gen/%.c
	@mkdir -p $(@D)
	$(Q) cc $(WHD_GEN_FLAGS) -c $< -o $@
build/whd_gen_obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(Q) cc $(WHD_GEN_FLAGS) -c $< -o $@
$(WHD_GEN): $(WHD_GEN_OBJ)
	$(Q) c++ $^ -o $@
data: $(WHD_GEN)
	@test -n "$(WAD)" || { echo "make data WAD=<the shareware DOOM1.WAD> (python3 tools/fetch.py downloads it)"; exit 1; }
	$(WHD_GEN) -silent -lossless $(WAD) src/doom1.whx

clean:
	$(Q) rm -rf $(BUILD_DIR) build
