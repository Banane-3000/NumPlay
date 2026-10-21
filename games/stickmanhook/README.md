# Stickman Hook

Native NumWorks adaptation by **Bobydeluxe**, based on the [reference game supplied for this port](https://github.com/Gr4ys0n/Gr4ys0n.github.io/tree/29006b0f11904704c390b457f7055c9fa1e53484/public/assets/games/stickman-hook).

The game runs as a standalone external app on N0110, N0115 and N0120. Its interface is entirely in English. This directory is independent of the launcher and the other locally developed games.

## Playing

| Key | Action |
| --- | --- |
| Hold OK, EXE or Shift | Attach to the nearest hook |
| Release | Let go and keep the swing's momentum |
| Back | Pause / resume |
| 0 during play | Retry the current level |
| Arrows + OK | Navigate menus, choose a level or equip a skin |
| 0 in the title menu | Controls |
| Home or Power | Save and exit |

Swing between hooks, use the spring bumpers, and cross the chequered finish line. Falling below the course automatically retries it. Holding the button keeps the same rope attached; release and press again to pick another hook.

The campaign has exactly **50 fixed levels**: the reference version's nine introductory levels and thirty specified combinations, followed by eleven fixed courses composed from its original sections. Level 50 ends the campaign; there is no level 51. Previously reached levels remain available from the level selector. The compact build includes only the 84 course parts used by this campaign, while the complete original section data remains available in the source assets.

All 23 original characters are included, with their original hook, free-flight and victory poses and dance frames. All characters can be unlocked within the campaign. Original level unlocks are adapted to milestones up to level 50. Characters originally tied to browser adverts unlock through progress, every three completed levels. Progress and the equipped skin are stored in the checked 40-byte `stickmanhook.sav` file. Saves from the earlier endless version migrate without resetting progress; progress beyond the new campaign is retained as a completed 50-level campaign. There are no ads or network dependencies.

## Fidelity and calculator adaptations

The port uses the supplied game's actual course coordinates, wall polygons, character frames, title artwork, bumpers, hooks, backgrounds, victory platform and JUNEGULL lettering. Physics retains gravity 1, player radius 0.017, four substeps per 1/60-second tick, 1.075 attachment momentum, 1.2 release momentum, the 0.2-second rope pull and the original collision response. The reference JavaScript physics functions are retained separately as a test oracle; they are not shipped in the app.

This is a native adaptation for a 320 × 240 RGB565 screen, rather than the browser runtime. Menus are arranged for physical keys. Artwork is resized and palette-packed; the trail and celebration use bounded drawing work. The small-screen camera keeps the original world scale and horizontal look-ahead. The victory screen uses the original platform and dancing characters. Simulation runs at 60 Hz and display updates target 30 Hz on every model. Floating-point rounding can differ from JavaScript doubles. A zero-length rope is guarded against to prevent invalid physics.

## Storage

The `.nwa` is **90,208 bytes**, down from 127,344 bytes (**29.16% smaller**). Repeated bumper parameters, rotations, palettes, pixel blocks and font glyphs are shared in flash. Unused sections are omitted from the installed image. These changes preserve the sprite pixels and physics values; there is no extra runtime decompression or RAM buffer.

## Build

Requires Python with Pillow, Node.js, `nwlink@1.0.0`, and an ARM embedded GCC toolchain. All required game assets and generated C tables are already included; building does not download the original game.

```sh
python build.py --arm-bin /path/to/arm/bin --nwlink /path/to/nwlink/bin/nwlink
```

The build creates `output/stickmanhook.nwa` and copies it to `build/apps/StickmanHook.nwa` at the repository root. A symbol-bearing `output/stickmanhook-debug.nwa` is kept for ARM tests. The installed release and debug bytes are compared automatically, using the smallest tested 107,674-byte app RAM budget.

The Makefile also supports:

```sh
make build check
make test HOST_CC=cc
make test-fidelity HOST_CC=cc
make test-arm NWLINK=nwlink
```

`test-arm` requires the dependencies used by NumPlay's `tools/emu.py`: Unicorn, pyelftools and Pillow. Run `make test` first to create its host fixtures. See [VALIDATION.md](VALIDATION.md).

## Asset provenance

The original game is Stickman Hook by Madbox. This fan adaptation is not affiliated with Madbox. The supplied files were pinned to commit `29006b0f11904704c390b457f7055c9fa1e53484`; hashes are recorded in `assets/provenance.json`. `assets/levels.json` and `assets/geometry.json` preserve the exported source data. `assets/sprites/` contains resized original frames, and `tools/assets.py` deterministically builds the ROM tables from those files.

To reproduce the import, put the pinned `bundle.js` and its referenced images in a local reference directory, then run:

```sh
node tools/extract_reference.cjs /path/to/reference
python tools/import_reference.py /path/to/reference
python tools/assets.py
```

Original game assets, lettering, course data and JavaScript excerpts retain their original rights; this adaptation does not relicense them. NumPlay's common app/file helpers and compact packaging tools retain their existing notices. Calculator adaptation credit: **By Bobydeluxe**.
