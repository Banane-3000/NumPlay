# Dune

Native NumWorks adaptation **By Bobydeluxe**, based on the supplied [Dune Rider reference by Inlogic Software](https://github.com/Gr4ys0n/Gr4ys0n.github.io/tree/29006b0f11904704c390b457f7055c9fa1e53484/public/assets/games/dune).

Ride the dunes, release before the climb to take off, and hold in the air to dive onto a downslope. Crossing the height line gives 1–3 points according to launch speed. Long, fast slides earn the original Nice/Perfect Slide bonuses and momentum boosts. A hard landing on an uphill face ends the run. This compact edition retains only the original **second landscape, Mountains**, now free and always active. The shop contains **seven balls**: Basic Ball, Basketball, 8-Ball, Football, Donut, Smiley and Rainbow Bubble. Their original coin prices and cosmetic behaviour are retained.

| Key | Action |
| --- | --- |
| Hold OK, EXE or Shift | Speed up downhill / dive in the air |
| Release | Take off and preserve momentum |
| Back | Pause / return |
| Arrows and OK | Menu, shop, purchase and equip |
| 0 on the title screen | Controls |
| Home or Power | Save and exit |

Ball sprites, the Mountains artwork, terrain textures, icons, rotation flags, trail patterns, score rules and retained shop entries come from the reference. UI/title lettering uses antialiased Segoe UI rasterized independently at 13, 22 and 32 pixels, with no bitmap enlargement and no displaced text shadow. Single-pass labels and solid HUD backgrounds improve contrast. There are no browser, audio, advert or network dependencies. Coins, record, purchases and equipped items persist in a versioned, checked **40-byte** save. Purchases ask for confirmation; equipping an owned item is free. Storage-full handling preserves other calculator files.

The `DUN2` save format imports the original `DUN1` file automatically. Record, coins, runs and retained purchases survive. A removed equipped ball returns to Basic Ball. Removed ball purchases and previously purchased landscapes are refunded once in coins; the refund saturates safely at the wallet limit. Subsequent saves use the new catalog indices and cannot repeat the refund.

## Native physics and rendering

The world retains the original 665-pixel height, gravity 1,000, physical ball radius 42, terrain formula and coarse collision segments. Input acceleration, speed thresholds, altitude corrections, score thresholds, slide boosts and landing conditions follow the supplied JavaScript. A bounded circle/segment contact solver with four substeps replaces Phaser/p2. It preserves the mechanics and momentum but is not a bit-for-bit copy of p2's iterative contact solver. The camera keeps the original zoom rules and a view ahead of the ball. Simulation runs at 60 Hz; display updates target 30 Hz on all three calculator models.

Endless terrain uses nine reusable shapes and eight active segments. Coins use a fixed 32-entry pool. Long runs periodically shift world coordinates together to retain floating-point precision. Rotating balls and coloured triangular trails remain animated, including the Donut's hollow trail after catalog reindexing. The sole background is resized, precomposed and slowly scrolled; the original browser's separate moving layers/particles are consolidated for memory and storage. Terrain and ball movement remain continuous.

The release file is **71,444 bytes**, down from 211,996 bytes (**66.3% smaller**), including the new native-size font coverage masks. Removed landscapes, textures, thumbnails and ball sprites are omitted from both the source assets and compiled image. The retained background uses row-indexed RLE and a 32-colour RGB565 palette; sprites/textures use four-bit palette indices, and the icon uses the compact NWA packer. The renderer uses one 5,120-byte strip and a 460-byte row decoder; it never expands a complete background or framebuffer into RAM. Static app RAM remains **8,173 bytes**. No heap allocation is used during play.

## Build and validation

Requires Python with Pillow, Node.js, `nwlink@1.0.0` and ARM embedded GCC. All imported PNG/JSON assets and generated C data are included. Building does not download the original game.

```sh
python build.py --arm-bin /path/to/arm/bin --nwlink /path/to/nwlink/bin/nwlink
make test HOST_CC=cc
make test-assets
make test-arm NWLINK=nwlink
make fidelity HOST_CC=cc REFERENCE_JS=/path/to/pinned/dune/js/game.js
```

The build produces `output/dune.nwa` and copies it to the repository's `build/apps/Dune.nwa`. Run host tests before ARM tests. The ARM tests use NumPlay's emulator and require Unicorn, pyelftools and Pillow. Fidelity tests execute the pinned original JavaScript's terrain generator and input handler, verifying their source digest first. They compare 100 full terrain curves/contact meshes and 1,000 control cases. These checks do not claim that the replacement contact solver reproduces every p2 trajectory. See [VALIDATION.md](VALIDATION.md).

The adaptation is local and independent of the launcher. Original game assets and data remain credited to their original authors and retain their rights. Inlogic's game is identified as Dune Rider, version 1.0.1, dated 20 April 2020 in its supplied script. Source revision/digest are recorded in `assets/provenance.json`; original catalog IDs are retained for migration. UI font masks are pre-baked in `assets/ui-fonts.json`; an installed font is not needed to build. `tools/import_fonts.py` regenerates these masks from supplied regular/bold TTF files. NumPlay's common app/file helpers and compact packaging tools retain their existing notices.

This fan adaptation is not affiliated with Inlogic Software. UI masks are rasterized from Microsoft Segoe UI; no TTF files are distributed. Borrowed artwork and lettering retain their original rights.
