# Validation

Validated locally on **2026-10-22** with ARM GCC 15.2.1, nwlink, the repository's
Unicorn ARM emulator, and native host tests. Physical devices were not used.

## Artifact and memory

| Measurement | Bytes |
| --- | ---: |
| Standalone `.nwa` | 547,612 |
| Installed application image | 530,620 |
| Static RAM, including renderer, game state and pathfinding | 20,489 |
| Maximum observed stack | 924 |
| Persistent save | 768 |

Release and debug artifacts produce identical installed application bytes.
The packer links successfully against the smallest tested app RAM allocation,
107,674 bytes. There is no gameplay heap, decoded texture atlas, Lua VM, web
engine or full-screen framebuffer. Rendering uses a 5,120-byte display strip.

Release SHA-256:

```text
9be24081603fc84a7bdf15847b506c24f83127c4062cb3caa370c6451bfd9dce
```

## Content and gameplay

`make test` verifies:

- The original $35 starting balance buys the checkout, first shelf and first
  banana crop. Harvesting, stocking, customers and checkout then earn money
  without granting extra funds.
- Both main purchase sequences finish: 17 milestones in Mart 1 and 18 in Mart 2.
  All 75 layout entries, including secondary unlocks and automatic machine/feed
  activations, become available.
- Every production chain works, including the two-input chocolate-bar machine.
  A further 36,000 ticks per mart exercise autonomous employee restocking and
  manager repairs; all 14 products are produced during these runs.
- All upgrade paths, hat choices, reward cooldowns and mart switching work.
- Save encoding round-trips and rejects truncation, absent data and corruption
  at every byte position.
- Every interface view renders through the same bounded strip callback.

The complete host run exercises **85,048 simulation ticks**. Generated saves of
both fully developed marts are loaded by the ARM tests.

`make test-assets` verifies original source/converted asset hashes, the two
complete layouts, distinct peanut-bed positions, all product IDs, 21 hat
choices, 204 baked animation frames, native-size fonts and deterministic asset
generation.

## ARM execution

| Profile | App RAM allocation | Startup, movement, pause, save, reload, exit |
| --- | ---: | --- |
| N0110 / Epsilon | 148,928 | Pass |
| N0115 / Epsilon | 133,652 | Pass |
| N0120 / Epsilon | 153,676 | Pass |
| N0110 / Upsilon | 107,674 | Pass |

Both fully developed marts are also exercised on all three Epsilon profiles.
The simulation runs at approximately 60 ticks per second, with a display target
of 30 frames per second. The tested complete scenes take at most 25 ms to draw
in the instruction-timed emulator. These are emulator measurements; actual LCD
transfer time and firmware can affect device frame rate.

The ARM suite additionally verifies a first checkout purchase using only real
key events, pause freezing the simulation, ignored launch-held keys, confirmed
reset and reset cancellation, full-storage retry/continue/exit behavior,
preservation of an unrelated `keep.py` record, firmware RAM guards, lock
balance, and app-RAM clearing on exit.

Detailed generated reports are in `output/build-report.json`,
`output/host-report.json` and `output/arm/report.json` after running the checks.

## Visual checks

Title, both marts, upgrades, hats, market selection, controls and storage-error
screens were inspected at the calculator's native 320 × 240 resolution. The
original sprites and logo are resampled before packing; menu text is rasterized
at its actual display size. `docs/` contains representative captures.

## Reference limits

The supplied build includes Mart 1 and Mart 2; Mart 3 is absent and disabled in
its original game-state module. Keyboard controls, scaled graphics, sampled
animation and offline advertising rewards are calculator adaptations, described
in the README. This validates the native adaptation, not pixel identity with
the original web renderer.
