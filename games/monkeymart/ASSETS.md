# Reference assets

The reference is the Defold build at commit
`29006b0f11904704c390b457f7055c9fa1e53484` in
`Gr4ys0n/Gr4ys0n.github.io`, directory
`public/assets/games/monkey-mart`. Its project version is `2.1`.
Original game and artwork are by **TinyDobbins**.

## Conversion

1. Read the version-4 Defold resource index and manifest, including the excluded
   live-update resources in `resources.zip`.
2. Verify each stored resource against its manifest SHA-1 before decoding.
3. Decode the public Defold XTEA-CTR resource format and LZ4 blocks. Recover Lua
   sources from the resource protobufs. These sources contain plain Lua text.
4. Evaluate the two `DataMart` initialization tables with inert engine stubs.
   Recover positions, collision polygons and animation names from compiled
   collection and sprite protobufs. Capture secondary unlocks, staff assignments
   and the generated scenery from the original functions.
5. Decode the six Basis Universal texture atlases with the official Basis
   transcoder. Apply each Defold texture-set UV rectangle and packing rotation
   to recover the source images.
6. Bake the original region-based Spine skeleton into animation frames offline.
   `tools/bake_actors.py` performs this step from a decoded reference directory.
   The native app uses player skin 1, customer skins 2–7, employee skin 9, the
   original cow and chicken, original hats and carried product sprites.
7. Store 4-bit, per-sprite palettes with transparent index zero. Share identical
   frames. Resize the source floors to the native world scale and encode each
   row with 16-color runs and packed literal pixels. Decoding writes directly
   into an eight-row display strip.

The original code gives the sixth peanut bed the fifth bed's name. This port
uses the actual `placebox_f9_6` collection position for the sixth bed so all six
peanut beds remain distinct.

`assets/reference.json` contains the recovered gameplay tables, source commit
and source version. `assets/actors.json` records each baked frame's source skin,
animation, frame index and anchor. `assets/provenance.json` records downloaded
archive hashes, original module hashes and converted asset hashes.

## Rebuilding local assets

```sh
python tools/assets.py
```

This rebuilds `src/assets.c`, `src/sprite-ids.h` and `src/data.c` using only the
checked-in PNG and JSON files. Original Defold, WASM, Lua and Basis libraries
are not linked into the application.

For a decoded reference directory containing `skeleton.json` and the original
`sprites/dodik__*.png` images:

```sh
python tools/bake_actors.py --reference /path/to/decoded/reference
python tools/assets.py
```

The optional animation baker needs NumPy and Pillow. It samples the original
region transforms and draw order; interpolation and frame rate are reduced for
the native display. Ordinary builds do not require NumPy.

`tools/import_fonts.py` can regenerate the native-size antialiased menu fonts
from explicitly supplied TrueType fonts. The baked glyphs are included, so the
build does not need system fonts.

No proprietary reference executable, web engine, analytics, advertising SDK,
audio decoder or downloaded development tool is packaged in the `.nwa`.
