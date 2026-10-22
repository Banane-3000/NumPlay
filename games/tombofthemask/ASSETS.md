# Reference and conversion

The supplied reference is pinned to commit
`29006b0f11904704c390b457f7055c9fa1e53484` in
`Gr4ys0n/Gr4ys0n.github.io`, directory `public/assets/games/tomb-of-the-mask`.
It is a Unity **2018.3.9f1** IL2CPP WebGL build. The game is credited to
Happymagenta / Playgendary; the supplied build's own links name both.

The reference needs a 256 MB web heap. This port does not include Unity,
WebAssembly, IL2CPP metadata, browser glue or the audio engine.

## Levels

Unity TextAssets contain 200 numbered TMX stage maps and 517 arcade sections.
All tile IDs and coordinates are retained. Only Tiled flip flags on wall tiles
are discarded; those cells remain the same wall type. All 200 maps were also
compared with the build's binary `stages` cache: reversing TMX row order produces
identical cell bytes for every stage, with no differences.

The largest stage has 1,720 cells. A stage and the 1,664-cell arcade window share
one 1,792-byte buffer. Nothing reserves the product of the largest width and
largest height: those dimensions belong to different maps.

Each map independently selects the smaller of:

- A nibble-coded run stream with a common tile palette and literal escapes.
- Bounded LZSS with a 4,096-byte look-back window and 3–18-byte matches.

The combined level stream is **59,424 bytes**, down from 64,863 bytes after the
first run packer. `assets/maps.json` records every map's dimensions, encoding,
offset and SHA-256 of its decoded cells. The native decoder checks source and
destination bounds. It uses no decompression heap or global history buffer.

## Pictures and fonts

UnityPy recovered the sprites from the original texture atlases. The checked-in
atlas contains 706 gameplay and logo sprites, including the 16 masks' animation
frames. Identical dimensions, coverage and colour planes share storage.

Each sprite keeps two one-bit planes: coverage and white ink. Black pixels are
preserved, including the characters' faces; they are not mistaken for transparent
pixels. Colours come from the original tint palettes. Alpha and colour thresholds
convert texture compression noise to the intended crisp monochrome pixels.

The UI font comes from `resources.assets` Font 813 and Texture 72. Punctuation
missing from that bitmap font is baked from the build's own Roboto Bold fallback,
Font 814. Only the needed glyph pixels are retained, with source font hashes in
`font-extra.json`. No external system font is required to rebuild the game.

## Rules

Il2CppDumper recovered type names and metadata; wabt exposed the relevant WASM
functions. The default variable initializer is WASM function 61084. Its original
keys and values are retained in `assets/rules.json`, without mobile service IDs
or advertising configuration. `DataManager`'s static initializer supplies the
0.12-unit tile size; the default `PlayerSpeed` is 5.

Serialized `DataManager` and `GameController` objects supply the 16 mask names,
unlock requirements, animation references and original colour palettes.
The port follows the reference defaults rather than any remotely changed values.

`assets/reference.json` records reference URLs and archive SHA-256 hashes.
`assets/provenance.json` records converted-file hashes. The conversion tools used
only for research are kept outside the game under the ignored
`build/tomb-reference/`; normal builds use the checked-in conversion and the
small reproducible `tools/assets.py` packer.
