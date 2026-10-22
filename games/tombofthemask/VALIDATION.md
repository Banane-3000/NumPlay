# Validation

Validated locally on **2026-10-22** with ARM GCC 15.2.1, nwlink, native C tests
and the repository's Unicorn ARM emulator. No physical calculator was used.

## Final artifact

| Measurement | Bytes |
| --- | ---: |
| Standalone `.nwa` | 139,292 |
| Installed image | 131,729 |
| Static application RAM | 11,661 |
| Maximum observed stack | 552 |
| Versioned persistent save | 320 |

SHA-256:

```text
11484910fac0075d2c4079a54f4f695c917e1930dc7531518d7f29958d73d375
```

The Python builder and `make check` produce identical release files. Release
and debug apps also produce identical installed bytes. Both link against the
smallest tested RAM allocation, **107,674 bytes**.

There is no gameplay heap, Unity runtime, decoded full-screen texture or
framebuffer. Rendering uses a 5,120-byte strip. Maps share a 1,792-byte buffer;
the largest original map uses 1,720 cells, and the endless window uses 1,664.
An endless run replaces sections in that window rather than growing it.

## Source fidelity

- All **200** decoded stage arrays match both their TMX cells and the original
  build's binary stage cache, accounting for row order.
- All **517** arcade sections match their recorded original tile hashes.
- The original four-star layouts of stages 90 and 117 remain intact. The saved
  star rating is capped at the three displayed slots.
- **706** original sprites, **16** masks, original palettes and the original
  bitmap font are packed reproducibly. Missing punctuation uses a supplied
  fallback font.
- Original default tables supply experience targets, mask levels and prices,
  mask advantages, upgrade prices/effects, mission goals and reward values.

These checks establish content preservation. They are not a claim of identical
Unity collision results, random sequences or mobile service behavior. The
native adaptations are described in [README.md](README.md).

## Native tests

`make test` checks:

- Loading all 717 maps; every stage has an entrance, an exit and its original
  star placements.
- Wall stopping, bounded coordinates, ice breaking, mixed-orientation portal
  pairs, temporary walls, pickups, shields and active hazards while standing.
- Every power-up, all upgrade levels, mask unlocks, mission claims, chest
  purchases/free cooldowns and reward persistence.
- Save round-trips, absent/truncated saves, and corruption at every byte of the
  320-byte record.
- **240,317** random-input simulation steps across the 200 stages, plus a
  controlled 12,000-step arcade-window replacement stress test.
- Rendering menus and gameplay through a bounded display callback.

`make test-assets` verifies converted-file hashes, all map hashes, original
content counts, important rule values and byte-for-byte reproducible generation.

## ARM execution

| Profile | App RAM allocation | Move, pause, save/reload and clean exit |
| --- | ---: | --- |
| N0120 / Epsilon | 153,676 | Pass |
| N0110 / Epsilon | 148,928 | Pass |
| N0115 / Epsilon | 133,652 | Pass |
| N0110 / Upsilon | 107,674 | Pass |

The suite also checks ignored launch-held keys, menu navigation, arcade, full
storage retry/continue/exit, preservation of another `keep.py` record, firmware
RAM guards, lock balance and application RAM clearing on exit.

A real-key script finishes stage 1 with all three stars, reaches the results
screen and verifies that stage 2 is unlocked in the saved record. Another
script opens a chest and verifies its reward and cooldown are saved.

The largest stage, 190, is exercised on N0120 and N0110 using a host-generated
progress save. Those runs produced **182 frames in six simulated seconds**,
a maximum measured draw time of **12 ms**, and no draws above 33 ms. The first
stage and menus also stayed within 12 ms. These are instruction-timed emulator
measurements; actual LCD transfer and firmware timing need physical testing.

All maps have been loaded and exercised with random input; all 200 stages have
not been played through to completion by a human. The end-to-end clear test
covers stage 1. Menu screenshots generated from advanced progress are fixtures.

Reports are generated in `output/build-report.json`, `output/host-report.json`
and `output/arm/report.json`. The root build target, Windows builder discovery
and standalone release catalogue are checked separately. The app is excluded
from the launcher and `games/games.json`.
