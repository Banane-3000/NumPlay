# Source provenance

| Project | Pinned revision | Use |
| --- | --- | --- |
| [FunkinCrew/Funkin](https://github.com/FunkinCrew/Funkin/tree/d05659bbdcf0d83073c900207cf410472afbbede) | `d05659bbdcf0d83073c900207cf410472afbbede` | Timing, note/score rules, original credits and license |
| [FunkinCrew/Funkin.art](https://github.com/FunkinCrew/Funkin.art/tree/429ab728b19272dca834c83d97b1646455b22579) | `429ab728b19272dca834c83d97b1646455b22579` | Original art project and chart format reference |
| [FunkinCrew/Funkin.assets](https://github.com/FunkinCrew/Funkin.assets/tree/724eba9de9e76761d9cefd52169926bc51c760a5) | `724eba9de9e76761d9cefd52169926bc51c760a5` | Published game charts, metadata, character/stage configs and compiled artwork |

The game source's asset/art submodule revisions match the user-provided links. Reference downloads were checked against their Git blob hashes and lengths. Music, sound effects, voices and video were neither imported into the game nor embedded in the `.nwa`. No ROM, browser runtime, web page or emulator of the original game is embedded.

`assets/model.json.zlib` preserves the original metadata, events and note arrays for all 61 song variations, plus converted character/stage/UI tables. `assets/chart-manifest.json` identifies the 166 selectable charts and their binary note stream hashes. `tests/test_assets.py` verifies every original time, duration, direction and kind against the compiled streams. `output/asset-report.json` records the exact hashes of the packed build inputs.

`tools/reference_art.py` reads Sparrow XML, packed text atlases and Adobe Animate JSON, including nested symbols, transformations, alpha and clipping masks. `tools/import_reference.py` extracts selected frames, crops transparency, resizes to calculator scale, quantizes to 15 opaque colours and transparency, and deduplicates exact frames. Character offsets are retained relative to their native idle centre and foot position. Stage props are composited into 160×120 cached backgrounds. Desktop shader colour filters and parallax are simplified. The game icon is derived from the original Boyfriend artwork.

Ordinary sprites use independently addressable row RLE with 4-bit palette indices. Stage/menu backgrounds use bounded raw DEFLATE and decode to exactly 9,600 bytes. Notes use delta timestamps and unsigned LEB128, with optional hold duration. No lossy chart simplification is used. The ARM build runs the same score and chart engine as the host tests.

Original license texts are in `LICENSES/Funkin.md`, `LICENSES/funkin.art.md` and `LICENSES/funkin.assets.md`. Nunito's font license is `LICENSES/OFL-1.1-Nunito.txt`. The upstream art and assets are proprietary; their conversion does not remove their copyright or permit public redistribution. The public contribution keeps these license texts and flags this restriction for maintainer review; it does not assert additional redistribution permission.
