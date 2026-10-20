# Jetpack Joyride: English-only app and NumPlay module.
CC = arm-none-eabi-gcc
NWLINK ?= npx --yes -- nwlink@1.0.0
PYTHON ?= python3
ARM_LD ?= arm-none-eabi-ld
OBJCOPY = arm-none-eabi-objcopy
PACKER = tools/compact_nwa.py tools/standalone.ld
SRC = src/main.c src/physics.c src/save.c src/render.c
HEADERS = $(wildcard src/*.h) ../common/epsilon_app.h ../common/epsilon_files.h
OBJECTS = $(patsubst src/%.c,output/%.o,$(SRC))
CFLAGS = -std=c11 $(shell $(NWLINK) eadk-cflags-device) -Os -Wall -Wextra -Werror
CFLAGS += -fno-math-errno -fsingle-precision-constant -ffunction-sections -fdata-sections -fstack-usage
CFLAGS += -fno-tree-loop-distribute-patterns -flto -fno-fat-lto-objects -fvisibility=internal
LINK = -nostartfiles --specs=nano.specs -Wl,--relocatable,--gc-sections,-e,main -flinker-output=nolto-rel

.PHONY: build check module sim-module test clean assets check-assets
build: output/jetpackjoyride.nwa
assets:
	$(PYTHON) tools/assets.py
check-assets:
	$(PYTHON) tools/assets.py --check
output/%.o: src/%.c $(HEADERS) | output
	$(CC) $(CFLAGS) -c $< -o $@
output/icon.o: src/icon.png | output
	$(NWLINK) png-icon-o $< $@
output/icon-compact.o: output/icon.o $(PACKER)
	$(PYTHON) tools/compact_nwa.py icon $< $@ --objcopy $(OBJCOPY)
output/jetpackjoyride-debug.nwa: $(OBJECTS) output/icon-compact.o $(PACKER)
	$(CC) $(CFLAGS) $(LINK) -Wl,-u,eadk_app_name,-u,eadk_app_icon,-u,eadk_api_level $(OBJECTS) output/icon-compact.o -lm -o output/standalone.o
	$(PYTHON) tools/compact_nwa.py app output/standalone.o $@ --ld $(ARM_LD)
output/jetpackjoyride.nwa: output/jetpackjoyride-debug.nwa $(PACKER)
	$(PYTHON) tools/compact_nwa.py release $< $@ --objcopy $(OBJCOPY)
module: output/module.o
output/module.o: $(OBJECTS)
	$(CC) $(CFLAGS) $(LINK) $^ -lm -o $@
check: build
	$(NWLINK) nwa-bin output/jetpackjoyride.nwa output/jetpackjoyride.bin --ram-length 107674
	$(NWLINK) nwa-bin output/jetpackjoyride-debug.nwa output/jetpackjoyride-debug.bin --ram-length 107674
	$(PYTHON) -c "from pathlib import Path; assert Path('output/jetpackjoyride.bin').read_bytes() == Path('output/jetpackjoyride-debug.bin').read_bytes(), 'Release compaction changed installed bytes'"
output:
	mkdir -p $@
test: | output
	$(PYTHON) tests/test_compact_nwa.py
	$(HOST_CC) -std=c11 -O2 -Wall -Wextra -Werror -Isrc tests/test.c src/physics.c src/save.c src/render.c -lm -o output/test
	cd output && ./test
HOST_CC ?= cc
sim-module: | output
	mkdir -p output/sim
	cc -std=c11 -O2 -fPIC $(shell $(NWLINK) eadk-cflags-simulator) -Dmain=np_jetpackjoyride_main -c src/main.c -o output/sim/main.o
	cc -std=c11 -O2 -fPIC -c src/physics.c -o output/sim/physics.o
	cc -std=c11 -O2 -fPIC -c src/save.c -o output/sim/save.o
	cc -std=c11 -O2 -fPIC -c src/render.c -o output/sim/render.o
	ld -r output/sim/*.o -exported_symbol _np_jetpackjoyride_main -o output/sim-module.o
clean:
	rm -rf output
