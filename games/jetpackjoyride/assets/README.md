# Artwork provenance and packing

The supplied [Jetpack Joyride Facebook assets archive](https://archive.org/download/jetpack-joyride-assets)
identifies **Halfbrick** as creator. Its `jetpack joyride images.rar` contains
the source PNGs used here. `source-manifest.json` records each original filename
and SHA-256, alongside its descriptive local filename in `source/`.

These are original game assets adapted for this unofficial NumWorks version:
Barry's separate head/body animations, machine-gun jetpack, title wordmarks,
title character, laboratory wall and floor, scientists, spinning coins,
electric beams, caps, missiles, muzzle flashes, warning and shield artwork.
The outfits recolour Barry's suit while retaining the original skin/hair.
The rotating beam uses the same electric colours and original endpoint sprites.

Artwork belongs to Halfbrick and retains its original rights. The archive does
not provide a separate redistribution licence; inclusion here does not place
these images under the GPL licence used for this implementation's code.
This fan adaptation is not affiliated with or endorsed by Halfbrick.

`python tools/assets.py` runs offline with Pillow and recreates `src/assets.h`,
`src/icon.png` and `assets/packed.json`. `--check` verifies those outputs without
writing. Normal ARM builds use the generated header and do not need Pillow.

Sprites are reduced to their display size and indexed palettes. Literal packets
use packed 4-bit indices where possible, with 8-bit indices for larger palettes.
Repeated colours and transparent spans use run-length packets. Every row has an
offset, allowing the renderer to decode only the current eight-pixel screen
strip. The generator round-trips every encoded row and validates all palette
indices and offsets. There is no heap allocation, texture-sized RAM buffer or
PNG decoder on the calculator. One small laboratory tile repeats at 2x scale;
it does not store multiple full-screen backgrounds.
