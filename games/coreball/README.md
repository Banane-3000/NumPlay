# Core Ball

Native NumWorks adaptation **By Bobydeluxe**, based on the [Core Ball version supplied for this port](https://github.com/Gr4ys0n/Gr4ys0n.github.io/tree/29006b0f11904704c390b457f7055c9fa1e53484/public/assets/games/core-ball).

The standalone application has the original **60 levels**, pin layouts, shot counts and rotation behaviours. Timing reversals, alternating speeds and the last levels' reversal on every shot are retained. Reached levels can be replayed from the level selector. Completing level 60 ends the campaign; it never creates level 61.

| Key | Action |
| --- | --- |
| Press OK, EXE or Shift | Shoot one pin |
| Back during a game | Pause |
| Arrows and OK | Menus and level selection |
| Home or Power | Save and exit |

The original grey title screen, Poiret One title, black playfield, white core and numbered pins are retained. Interface text uses antialiased Segoe UI glyphs rasterized separately at their actual 13/22/32-pixel sizes; headings are never enlarged from small bitmaps. The title and GO artwork also retain four-bit edge coverage. Menus use physical-key navigation and the 320 × 240 layout. Simulation runs at 60 Hz and display updates target 30 Hz, using the same timing on every model. Holding the shoot key does not repeatedly fire.

## Calculator adaptation

The world is scaled from the reference's 560-pixel height. Collision rounding is performed at that reference scale: rounding up the already reduced pin radii would close valid gaps in the last levels. Shots take 50 ms. Failure and completion retain the red/green transition and one-second delay. Rotation state resets for each attempt, so time spent paused or in a menu cannot unexpectedly change it. The original browser's persistent body-click listeners are replaced by game input; menus do not reverse a later level's rotation.

The release file is **35,692 bytes**, including the sharper native-size lettering and antialiased title masks. Lettering uses compact four-bit coverage masks, and level parameters occupy 180 bytes. Static RAM remains **5,557 bytes**. The display uses a single **5,120-byte strip**, without a full-screen framebuffer or heap allocation. Off-screen victory spokes are clipped before drawing to keep animation work bounded. Progress uses a versioned, checked 24-byte `coreball.sav` file. Storage-full handling offers retry, continued play or exit without overwriting other files.

## Build and validation

Requires Python with Pillow, Node.js, `nwlink@1.0.0` and ARM embedded GCC. Included PNG/JSON assets and generated C data are sufficient; building does not download the browser game.

```sh
python build.py --arm-bin /path/to/arm/bin --nwlink /path/to/nwlink/bin/nwlink
make test HOST_CC=cc
make test-arm NWLINK=nwlink
make fidelity HOST_CC=cc REFERENCE_JS=/path/to/pinned/core-ball/js/index.js
```

The build produces `output/coreball.nwa` and copies it to the repository's `build/apps/CoreBall.nwa`. Run the host tests before ARM tests to generate fixtures. ARM tests require NumPlay's emulator dependencies: Unicorn, pyelftools and Pillow. The fidelity test executes the original JavaScript's actual level/motion functions and checks its SHA-256 before comparing 66,000 samples. See [VALIDATION.md](VALIDATION.md).

The adaptation is local and independent of the NumPlay launcher. The original artwork, lettering and game data retain their original rights; the port does not relicense them. Poiret One is the reference's embedded title font. UI font masks are pre-baked in `assets/ui-fonts.json`; an installed font is not needed to build. `tools/import_fonts.py` can regenerate those masks from supplied regular/bold TTF files. Source revision and digest are recorded in `assets/provenance.json`. NumPlay's common app/file helpers and compact packaging tools retain their existing notices.

This fan adaptation is not affiliated with the original Core Ball creators. UI masks are rasterized from Microsoft Segoe UI; no TTF files are distributed. Borrowed artwork and lettering retain their original rights.
