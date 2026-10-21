# NumPlay: one launcher app with every game, plus each game on its own.
#
#   make            build/data/NumPlay*.nwa and build/compiled/*.nwa
#   make check      link the NumPlay apps like the calculator does, print sizes, check RAM
#   make sim        build/data/NumPlay.nwb for the Epsilon simulator
#   make emu        run NumPlay.nwa in the ARM emulator (tools/emulator.py)
#   make clean
#
# Needs arm-none-eabi-gcc (with newlib), Node.js for nwlink, Python 3 with
# Pillow, and Rust with the thumbv7em-none-eabihf target for Tetris.

NWLINK ?= npx --yes -- nwlink@1.0.0
ARM_CC = arm-none-eabi-gcc
ARM_LD = arm-none-eabi-ld
ARM_OBJCOPY = arm-none-eabi-objcopy
PY ?= python3
CARGO ?= cargo
B = build/data
# Conventional game Makefiles honor BUILD_DIR or OUT; unusual games remain local.
GAMES_REDIRECT = balatro blockblast breakout buckshot celeste championisland chess connectfour cookieclicker dinorun doom finalearth2 flappybird fnaf g2048 hillclimb hollowknight invaders isaac leveldevil minesweeper numblocks numpowder numvisuals pacman portal pvz snake sokoban solitaire sonic subway sudoku twoplayer undertale uttt
GAMES_OUT = dinorun
GAME_REDIRECT_ARG = $(if $(filter $(1),$(GAMES_OUT)),OUT=../../$(B)/games/$(1),BUILD_DIR=../../$(B)/games/$(1))
# the RAM the oldest calculator software that runs apps gives them (23.2: 148928 bytes; 25.2 gives 153676).
# NumPlay itself takes less: a game goes on past it into the RAM the software has left (see
# tools/gen_games.py), so it installs with less too, custom builds of the software say, where a game that
# needs more says so. The check: every game fits in RAM_LIMIT.
RAM_LIMIT = 148928
# custom builds of Epsilon 25.2's source give apps less: 133652 bytes; every game fits there too, in NumPlay and on
# its own (their big buffers are on the stack, 32 KB apart from that RAM, or in the RAM the software has left)
CUSTOM_RAM = 133652
BIG_APPS = Celeste HollowKnight Undertale ChampionIsland FinalEarth2 NumPowder NumBlocks NumDrive SubwaySurfers FiveNightsAtFreddys Doom BindingOfIsaac HillClimbRacing PlantsVsZombies SonicTheHedgehog CookieClicker LevelDevil
# the calculator's space for installed apps (0x90200000 to 0x903F0000 in its flash)
APP_SPACE = 2031616
# Upsilon, the N0110 and N0115's custom software, gives .nwa apps less RAM: NumPlay-Upsilon.nwa has
# the games that fit it (NumPlay draws their text there: see launcher/src/compat.c)
UPSILON_RAM = 107674
UPSILON_GAMES = crossyroad blockblast portal tetris dinorun chess flappybird pacman snake connectfour solitaire g2048 minesweeper \
  breakout twoplayer invaders sudoku uttt sokoban

# NumPlay and its discreet versions: the same app, with another name and icon
# on the calculator's home screen. Each one is a release file.
VARIANTS = NumPlay NumPlay-Invisible NumPlay-Matrices
NAME_NumPlay = "NumPlay"
ICON_NumPlay = launcher/assets/icon.png
NAME_NumPlay-Invisible = " "
ICON_NumPlay-Invisible = launcher/assets/icon-invisible.png
NAME_NumPlay-Matrices = "Matrices"
ICON_NumPlay-Matrices = launcher/assets/icon-matrices.png

EADK_CFLAGS := $(shell $(NWLINK) eadk-cflags-device)
EADK_SIM_CFLAGS := $(shell $(NWLINK) eadk-cflags-simulator)

.SECONDEXPANSION:

# Per-game facts (order, screenshots) come from games/games.json.
$(B)/games.mk: games/games.json tools/games_mk.py | $(B)
	$(PY) tools/games_mk.py $< > $@
-include $(B)/games.mk

LAUNCHER_SRC = $(wildcard launcher/src/*.c)
LAUNCHER_H = $(wildcard launcher/src/*.h)
# the release version, shown in NumPlay's settings (a release is tagged v$(VERSION))
VERSION := $(shell cat VERSION)
ARM_CFLAGS = -std=gnu11 $(EADK_CFLAGS) -DNP_VERSION='"$(VERSION)"' -Os -Wall -Wextra -Wno-unused-parameter -fno-math-errno \
  -fno-tree-loop-distribute-patterns -ffunction-sections -fdata-sections -Ilauncher/src
ARM_LINK = -nostartfiles --specs=nano.specs -Wl,--relocatable -Wl,--gc-sections -Wl,-e,main \
  -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level

.PHONY: all nwa apps check sim emu clean lang FORCE tetris-app
all: nwa apps
nwa: $(foreach v,$(VARIANTS),$(B)/$(v).nwa) $(B)/NumPlay-Upsilon.nwa

$(B):
	mkdir -p $(B)/modules $(B)/gen $(B)/arm $(B)/apps $(B)/sim

# ------------------------------------------------------------------ modules
# Each game builds itself as a partially linked object (its own Makefile's
# `module` target), then becomes a NumPlay module: one block of flash, its RAM
# moved into the shared arena, `main` renamed np_<game>_main.
MOD_numdash = games/numdash/build/module.o
MOD_crossyroad = games/crossyroad/output/module.o
MOD_numdrive = games/numdrive/output/device/module.o
MOD_balatro = $(B)/games/balatro/module.o
MOD_buckshot = $(B)/games/buckshot/module.o
MOD_portal = $(B)/games/portal/module.o
MOD_chess = $(B)/games/chess/module.o
MOD_tetris = $(B)/tetris/raw.o
ENTRY = main
ENTRY_tetris = np_tetris_main

$(B)/tetris/raw.o: FORCE | $(B)
	mkdir -p $(B)/tetris
	cd games/tetris/tetris && NWLINK="$(NWLINK)" $(CARGO) build --release --quiet
	$(ARM_LD) -r -z noexecstack --gc-sections -e np_tetris_main -u np_tetris_main \
	  games/tetris/tetris/target/thumbv7em-none-eabihf/release/libtetris.a -o $@

$(B)/modules/%.o: FORCE | $(B)
	@if [ "$*" != tetris ]; then $(MAKE) --no-print-directory -C games/$* module NWLINK="$(NWLINK)" $(if $(filter $*,$(GAMES_REDIRECT)),$(call GAME_REDIRECT_ARG,$*),); \
	else $(MAKE) --no-print-directory $(MOD_tetris); fi
	$(ARM_LD) -r -d -T tools/module.ld $(MOD_$*) -o $(B)/modules/$*.1.o
	$(ARM_OBJCOPY) --keep-global-symbol=$(or $(ENTRY_$*),$(ENTRY)) \
	  --redefine-sym eadk_keyboard_scan=np_keyboard_scan --redefine-sym eadk_event_get=np_event_get \
	  --redefine-sym eadk_display_draw_string=np_display_draw_string \
	  $(B)/modules/$*.1.o $(B)/modules/$*.2.o
	$(PY) tools/npmodule.py $(B)/modules/$*.2.o $@ --game $* --index $(INDEX_$*) \
	  --entry $(or $(ENTRY_$*),$(ENTRY)) --json $(B)/modules/$*.json

# games in the standard layout (see tools/games_mk.py)
$(foreach g,$(STD_GAMES),$(eval MOD_$(g) = $(B)/games/$(g)/module.o)$(eval SIM_$(g) = $(B)/games/$(g)/sim-module.o))

MODULES = $(foreach g,$(GAMES),$(B)/modules/$(g).o)

# ------------------------------------------------------------------ generated glue
$(B)/gen/gametable.c: games/games.json tools/gen_games.py $(MODULES)
	$(PY) tools/gen_games.py games/games.json $(B)/modules $(B)/gen

$(B)/gen/shots_%.c: games/games.json tools/shots.py $$(SHOTS_$$*) | $(B)
	cd games/$* && $(PY) ../../tools/shots.py --id $* --index $(INDEX_$*) $(COLORS_$*) ../../$@ $(SHOTS_REL_$*)

# ------------------------------------------------------------------ NumPlay.nwa
ARM_OBJS = $(patsubst launcher/src/%.c,$(B)/arm/%.o,$(LAUNCHER_SRC)) $(B)/arm/gametable.o $(B)/arm/arena.o \
  $(B)/arm/marks.o $(foreach g,$(GAMES),$(B)/arm/shots_$(g).o)

$(B)/arm/%.o: launcher/src/%.c $(LAUNCHER_H) VERSION | $(B)
	$(ARM_CC) $(ARM_CFLAGS) -flto -c $< -o $@
$(B)/arm/gametable.o: $(B)/gen/gametable.c launcher/src/np.h
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@
$(B)/arm/shots_%.o: $(B)/gen/shots_%.c launcher/src/np.h
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@
$(B)/arm/arena.o $(B)/arm/marks.o: $(B)/arm/%.o: $(B)/gen/gametable.c
	$(ARM_CC) $(EADK_CFLAGS) -c $(B)/gen/$*.s -o $@
.SECONDARY: $(foreach v,$(VARIANTS),$(B)/variant/$(v)-name.o $(B)/variant/$(v)-icon.o) \
  $(foreach g,$(GAMES),$(B)/arm/shots_$(g).o)
$(B)/variant/%-name.o: launcher/app_name.c Makefile | $(B)
	mkdir -p $(B)/variant
	$(ARM_CC) $(ARM_CFLAGS) '-DNP_APP_NAME=$(NAME_$*)' -c $< -o $@
$(B)/variant/%-icon.o: $$(ICON_$$*) | $(B)
	mkdir -p $(B)/variant
	$(NWLINK) png-icon-o $< $@

$(B)/%.nwa: $(ARM_OBJS) $(MODULES) $(B)/variant/%-name.o $(B)/variant/%-icon.o
	$(ARM_CC) $(ARM_CFLAGS) $(ARM_LINK) -Wl,-T,$(B)/gen/numplay.ld -flinker-output=nolto-rel \
	  $(ARM_OBJS) $(MODULES) $(B)/variant/$*-name.o $(B)/variant/$*-icon.o -lm -lgcc -o $@
	arm-none-eabi-strip --strip-unneeded $@
	@if [ "$*" = NumPlay ]; then $(PY) tools/sizes.py $@ $(B)/modules; fi

check: nwa apps
	@x=$$(cat $(B)/gen/arena_extra); \
	  $(NWLINK) nwa-bin --ram-length $$(( $(CUSTOM_RAM) - x )) $(B)/NumPlay.nwa $(B)/NumPlay-custom.bin >/dev/null || \
	  { echo "a game of NumPlay needs more RAM than custom builds of 25.2 give apps ($(CUSTOM_RAM) bytes)"; exit 1; }
	@for a in $(BIG_APPS); do $(NWLINK) nwa-bin --ram-length $(CUSTOM_RAM) $(B)/apps/$$a.nwa $(B)/apps/$$a.bin >/dev/null || \
	  { echo "$$a.nwa needs more RAM than custom builds of 25.2 give apps ($(CUSTOM_RAM) bytes)"; exit 1; }; done; \
	  echo "NumPlay's games and $(BIG_APPS) fit in $(CUSTOM_RAM) bytes of RAM"
	@x=$$(cat $(B)/gen-upsilon/arena_extra); \
	  $(NWLINK) nwa-bin --ram-length $$(( $(UPSILON_RAM) - x )) $(B)/NumPlay-Upsilon.nwa $(B)/NumPlay-Upsilon.bin && \
	  echo "NumPlay-Upsilon.nwa installs as $$(wc -c < $(B)/NumPlay-Upsilon.bin) bytes; its RAM and any game's fit in $(UPSILON_RAM) bytes"
	@x=$$(cat $(B)/gen/arena_extra); \
	  for v in $(VARIANTS); do $(NWLINK) nwa-bin --ram-length $$(( $(RAM_LIMIT) - x )) $(B)/$$v.nwa $(B)/$$v.bin || exit 1; \
	  n=$$(wc -c < $(B)/$$v.bin); \
	  echo "$$v.nwa installs as $$n bytes ($$(( $(APP_SPACE) - n )) to spare); its RAM and any game's fit in $(RAM_LIMIT) bytes"; \
	  [ $$n -le $(APP_SPACE) ] || { echo "$$v.nwa is bigger than the calculator's app space ($(APP_SPACE) bytes)"; exit 1; }; done

# ------------------------------------------------------------------ NumPlay-Upsilon.nwa
$(B)/gen-upsilon/gametable.c: games/games.json tools/gen_games.py $(foreach g,$(UPSILON_GAMES),$(B)/modules/$(g).o)
	$(PY) tools/gen_games.py games/games.json $(B)/modules $(B)/gen-upsilon --only "$(UPSILON_GAMES)"
$(B)/arm-upsilon/gametable.o: $(B)/gen-upsilon/gametable.c launcher/src/np.h
	mkdir -p $(B)/arm-upsilon
	$(ARM_CC) $(ARM_CFLAGS) -c $< -o $@
$(B)/arm-upsilon/arena.o $(B)/arm-upsilon/marks.o: $(B)/arm-upsilon/%.o: $(B)/gen-upsilon/gametable.c
	mkdir -p $(B)/arm-upsilon
	$(ARM_CC) $(EADK_CFLAGS) -c $(B)/gen-upsilon/$*.s -o $@
UPSILON_OBJS = $(patsubst launcher/src/%.c,$(B)/arm/%.o,$(LAUNCHER_SRC)) $(B)/arm-upsilon/gametable.o \
  $(B)/arm-upsilon/arena.o $(B)/arm-upsilon/marks.o $(foreach g,$(UPSILON_GAMES),$(B)/arm/shots_$(g).o) \
  $(foreach g,$(UPSILON_GAMES),$(B)/modules/$(g).o)
$(B)/NumPlay-Upsilon.nwa: $(UPSILON_OBJS) $(B)/variant/NumPlay-name.o $(B)/variant/NumPlay-icon.o
	$(ARM_CC) $(ARM_CFLAGS) $(ARM_LINK) -Wl,-T,$(B)/gen-upsilon/numplay.ld -flinker-output=nolto-rel \
	  $^ -lm -lgcc -o $@
	arm-none-eabi-strip --strip-unneeded $@

# ------------------------------------------------------------------ NumPlay in other languages
# tools/lang.py copies what builds NumPlay to build/lang/<code> with the translations in place
# (launcher/lang/<code>.txt, games/<game>/lang/<code>.txt), and NumPlay is built there.
LANGS = fr zh
LANG_fr = French
LANG_zh = Chinese
lang: $(foreach l,$(LANGS),$(B)/NumPlay-$(LANG_$(l)).nwa)
$(foreach l,$(LANGS),$(B)/NumPlay-$(LANG_$(l)).nwa): $(B)/NumPlay-%.nwa: FORCE | $(B)
	$(eval L := $(if $(filter French,$*),fr,zh))
	$(PY) tools/lang.py tree $(L) $(B)/lang/$(L)
	$(MAKE) -C $(B)/lang/$(L) build/data/NumPlay.nwa NWLINK="$(NWLINK)"
	$(NWLINK) nwa-bin --ram-length $$(( $(CUSTOM_RAM) - $$(cat $(B)/lang/$(L)/build/data/gen/arena_extra) )) \
	  $(B)/lang/$(L)/build/data/NumPlay.nwa $(B)/lang/$(L)/build/data/NumPlay.bin
	@n=$$(wc -c < $(B)/lang/$(L)/build/data/NumPlay.bin); echo "NumPlay-$*.nwa installs as $$n bytes ($$(( $(APP_SPACE) - n )) to spare)"; \
	  [ $$n -le $(APP_SPACE) ] || { echo "NumPlay-$*.nwa is bigger than the calculator's app space"; exit 1; }
	cp $(B)/lang/$(L)/build/data/NumPlay.nwa $@

# ------------------------------------------------------------------ games on their own
APP_numdash = games/numdash/build/numdash.nwa:NumDash.nwa
APP_crossyroad = games/crossyroad/output/crossyroad.nwa:CrossyRoad.nwa
APP_numdrive = games/numdrive/output/device/numdrive.nwa:NumDrive.nwa
APP_balatro = $(B)/games/balatro/balatro.nwa:Balatro.nwa
APP_buckshot = $(B)/games/buckshot/buckshot.nwa:BuckshotRoulette.nwa
APP_portal = $(B)/games/portal/portal.nwa:PortalReturns.nwa
APP_chess = $(B)/games/chess/chess.nwa:NumChess.nwa
APP_tetris = games/tetris/tetris/target/thumbv7em-none-eabihf/release/tetris:Tetris.nwa

# Standalone Rust Tetris, without the full `apps` / launcher build.
tetris-app: | $(B)
	cd games/tetris/tetris && NWLINK="$(NWLINK)" $(CARGO) build --release --quiet
	cp $(word 1,$(subst :, ,$(APP_tetris))) $(B)/apps/Tetris.nwa
	arm-none-eabi-strip --strip-unneeded $(B)/apps/Tetris.nwa
APP_numvisuals = $(B)/games/numvisuals/numvisuals.nwa:NumVisuals.nwa
# too big to share the calculator's app space with NumPlay: only on their own
APP_celeste = $(B)/games/celeste/celeste.nwa:Celeste.nwa
APP_championisland = $(B)/games/championisland/championisland.nwa:ChampionIsland.nwa
APP_hollowknight = $(B)/games/hollowknight/hollowknight.nwa:HollowKnight.nwa
APP_undertale = $(B)/games/undertale/undertale.nwa:Undertale.nwa
APP_subway = $(B)/games/subway/subway.nwa:SubwaySurfers.nwa
APP_fnaf = $(B)/games/fnaf/fnaf.nwa:FiveNightsAtFreddys.nwa
APP_hillclimb = $(B)/games/hillclimb/hillclimb.nwa:HillClimbRacing.nwa
APP_finalearth2 = $(B)/games/finalearth2/finalearth2.nwa:FinalEarth2.nwa
APP_numpowder = $(B)/games/numpowder/numpowder.nwa:NumPowder.nwa
APP_doom = $(B)/games/doom/doom.nwa:Doom.nwa
APP_isaac = $(B)/games/isaac/isaac.nwa:BindingOfIsaac.nwa
APP_pvz = $(B)/games/pvz/pvz.nwa:PlantsVsZombies.nwa
APP_sonic = $(B)/games/sonic/sonic.nwa:SonicTheHedgehog.nwa
APP_cookieclicker = $(B)/games/cookieclicker/cookieclicker.nwa:CookieClicker.nwa
APP_leveldevil = $(B)/games/leveldevil/leveldevil.nwa:LevelDevil.nwa

apps: | $(B)
	$(MAKE) -C games/numdash build NWLINK="node node_modules/nwlink/bin/nwlink"
	$(MAKE) -C games/crossyroad NWLINK="$(NWLINK)"
	$(MAKE) -C games/numdrive NWLINK="$(NWLINK)"
	$(MAKE) -C games/balatro build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/balatro
	$(MAKE) -C games/buckshot build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/buckshot
	$(MAKE) -C games/portal build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/portal
	$(MAKE) -C games/chess build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/chess
	$(MAKE) -C games/numvisuals build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/numvisuals
	$(MAKE) -C games/celeste build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/celeste
	$(MAKE) -C games/championisland build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/championisland
	$(MAKE) -C games/hollowknight build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/hollowknight
	$(MAKE) -C games/undertale build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/undertale
	$(MAKE) -C games/subway build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/subway
	$(MAKE) -C games/fnaf build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/fnaf
	$(MAKE) -C games/hillclimb build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/hillclimb
	$(MAKE) -C games/finalearth2 build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/finalearth2
	$(MAKE) -C games/numpowder build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/numpowder
	$(MAKE) -C games/doom build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/doom
	$(MAKE) -C games/isaac build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/isaac
	$(MAKE) -C games/pvz build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/pvz
	$(MAKE) -C games/sonic build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/sonic
	$(MAKE) -C games/cookieclicker build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/cookieclicker
	$(MAKE) -C games/leveldevil build NWLINK="$(NWLINK)" BUILD_DIR=../../$(B)/games/leveldevil
	cd games/tetris/tetris && NWLINK="$(NWLINK)" $(CARGO) build --release --quiet
	cp $(word 1,$(subst :, ,$(APP_numdash))) $(B)/apps/$(word 2,$(subst :, ,$(APP_numdash)))
	cp $(word 1,$(subst :, ,$(APP_crossyroad))) $(B)/apps/$(word 2,$(subst :, ,$(APP_crossyroad)))
	cp $(word 1,$(subst :, ,$(APP_numdrive))) $(B)/apps/$(word 2,$(subst :, ,$(APP_numdrive)))
	cp $(word 1,$(subst :, ,$(APP_balatro))) $(B)/apps/$(word 2,$(subst :, ,$(APP_balatro)))
	cp $(word 1,$(subst :, ,$(APP_buckshot))) $(B)/apps/$(word 2,$(subst :, ,$(APP_buckshot)))
	cp $(word 1,$(subst :, ,$(APP_portal))) $(B)/apps/$(word 2,$(subst :, ,$(APP_portal)))
	cp $(word 1,$(subst :, ,$(APP_chess))) $(B)/apps/$(word 2,$(subst :, ,$(APP_chess)))
	cp $(word 1,$(subst :, ,$(APP_tetris))) $(B)/apps/$(word 2,$(subst :, ,$(APP_tetris)))
	cp $(word 1,$(subst :, ,$(APP_numvisuals))) $(B)/apps/$(word 2,$(subst :, ,$(APP_numvisuals)))
	cp $(word 1,$(subst :, ,$(APP_celeste))) $(B)/apps/$(word 2,$(subst :, ,$(APP_celeste)))
	cp $(word 1,$(subst :, ,$(APP_championisland))) $(B)/apps/$(word 2,$(subst :, ,$(APP_championisland)))
	cp $(word 1,$(subst :, ,$(APP_hollowknight))) $(B)/apps/$(word 2,$(subst :, ,$(APP_hollowknight)))
	cp $(word 1,$(subst :, ,$(APP_undertale))) $(B)/apps/$(word 2,$(subst :, ,$(APP_undertale)))
	cp $(word 1,$(subst :, ,$(APP_subway))) $(B)/apps/$(word 2,$(subst :, ,$(APP_subway)))
	cp $(word 1,$(subst :, ,$(APP_fnaf))) $(B)/apps/$(word 2,$(subst :, ,$(APP_fnaf)))
	cp $(word 1,$(subst :, ,$(APP_hillclimb))) $(B)/apps/$(word 2,$(subst :, ,$(APP_hillclimb)))
	cp $(word 1,$(subst :, ,$(APP_finalearth2))) $(B)/apps/$(word 2,$(subst :, ,$(APP_finalearth2)))
	cp $(word 1,$(subst :, ,$(APP_numpowder))) $(B)/apps/$(word 2,$(subst :, ,$(APP_numpowder)))
	cp $(word 1,$(subst :, ,$(APP_doom))) $(B)/apps/$(word 2,$(subst :, ,$(APP_doom)))
	cp $(word 1,$(subst :, ,$(APP_isaac))) $(B)/apps/$(word 2,$(subst :, ,$(APP_isaac)))
	cp $(word 1,$(subst :, ,$(APP_pvz))) $(B)/apps/$(word 2,$(subst :, ,$(APP_pvz)))
	cp $(word 1,$(subst :, ,$(APP_sonic))) $(B)/apps/$(word 2,$(subst :, ,$(APP_sonic)))
	cp $(word 1,$(subst :, ,$(APP_cookieclicker))) $(B)/apps/$(word 2,$(subst :, ,$(APP_cookieclicker)))
	cp $(word 1,$(subst :, ,$(APP_leveldevil))) $(B)/apps/$(word 2,$(subst :, ,$(APP_leveldevil)))
	arm-none-eabi-strip --strip-unneeded $(B)/apps/Tetris.nwa

STD_APPS = $(foreach g,$(STD_GAMES),$(B)/apps/$(NWA_$(g)))
apps: $(STD_APPS)
$(STD_APPS): $(B)/apps/%.nwa: FORCE | $(B)
	$(MAKE) --no-print-directory -C games/$(APPID_$*) build NWLINK="$(NWLINK)" $(call GAME_REDIRECT_ARG,$(APPID_$*))
	cp $(B)/games/$(APPID_$*)/$(APPID_$*).nwa $@

# ------------------------------------------------------------------ simulator
SIM_numdash = games/numdash/build/sim-module.o
SIM_crossyroad = games/crossyroad/output/sim-module.o
SIM_numdrive = games/numdrive/output/sim-module.o
SIM_balatro = $(B)/games/balatro/sim-module.o
SIM_buckshot = $(B)/games/buckshot/sim-module.o
SIM_portal = $(B)/games/portal/sim-module.o
SIM_chess = $(B)/games/chess/sim-module.o
SIM_tetris = $(B)/sim/tetris.o
SIM_CFLAGS = -std=gnu11 -O2 -fPIC -DNP_SIMULATOR=1 -DNP_VERSION='"$(VERSION)"' $(EADK_SIM_CFLAGS) -Ilauncher/src -Wall -Wno-unused-parameter

$(B)/sim/tetris.o: FORCE | $(B)
	cd games/tetris/tetris && NWLINK="$(NWLINK)" $(CARGO) build --lib --release --quiet --target aarch64-apple-darwin
	cc -r -nostdlib -arch arm64 -Wl,-u,_np_tetris_main -Wl,-exported_symbol,_np_tetris_main \
	  games/tetris/tetris/target/aarch64-apple-darwin/release/libtetris.a -o $@

$(B)/sim/%.mod: FORCE | $(B)
	@if [ "$*" != tetris ]; then $(MAKE) --no-print-directory -C games/$* sim-module NWLINK="$(NWLINK)" $(if $(filter $*,$(GAMES_REDIRECT)),$(call GAME_REDIRECT_ARG,$*),); \
	else $(MAKE) --no-print-directory $(SIM_tetris); fi

$(B)/sim/gen/gametable.c: games/games.json tools/gen_games.py $(MODULES)
	$(PY) tools/gen_games.py games/games.json $(B)/modules $(B)/sim/gen --simulator

sim: $(B)/NumPlay.nwb
$(B)/NumPlay.nwb: $(LAUNCHER_SRC) $(LAUNCHER_H) $(B)/sim/gen/gametable.c $(foreach g,$(GAMES),$(B)/gen/shots_$(g).c) \
  $(foreach g,$(GAMES),$(B)/sim/$(g).mod)
	cc $(SIM_CFLAGS) -shared -undefined dynamic_lookup $(LAUNCHER_SRC) launcher/sim/*.c $(B)/sim/gen/gametable.c \
	  $(foreach g,$(GAMES),$(B)/gen/shots_$(g).c) $(foreach g,$(GAMES),$(SIM_$(g))) -o $@

# ------------------------------------------------------------------ tools
emu: $(B)/NumPlay.nwa
	$(PY) tools/emulator.py $< --ms 4000 --shots 3000 --out $(B)/emu

clean:
	rm -rf $(B) build/compiled
