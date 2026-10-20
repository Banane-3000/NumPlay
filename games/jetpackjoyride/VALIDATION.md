# Local validation - 20 October 2026

Standalone Jetpack Joyride with the revised artwork/interface. Publication
changes are limited to this game and its link/credit in the root README.

## Artwork and interface

Forty packed sprites/textures are generated offline from the supplied Halfbrick
archive: Barry head/body frames, machine-gun jetpack, title wordmarks/character,
laboratory wall/floor, scientists, coins, electricity, caps, missile, muzzle
flash, warning and shield. Source filenames/hashes and rights are recorded in
`assets/source-manifest.json` and `assets/README.md`.

The lab wall stops at the physical floor. Barry and the background scientists
stand above that floor, and the foreground rail covers art below it. The HUD
covers all its first three strips so moving sprites cannot leave stale pixels
at its lower edge. The title's original wordmarks, character, slow laboratory
scroll and large gold/steel buttons replace the previous console. The Stash,
guide, ready, pause, restart, purchase, result and storage-error screens use
the new interface. English text, by Bobydeluxe credit and all controls remain.

The asset generator verifies the original PNG hashes and round-trips every
encoded row, checking offsets and palette indices. `tools/assets.py --check`
passes, verifying byte-identical header, resource report and launcher icon.
Normal builds use the generated header and do not require Pillow.

Small palettes use packed 4-bit literal indices; other sprites use 8-bit
indices. Repeated colours/transparency use RLE packets. Row offsets let the
renderer read only the current eight-pixel strip. The wall and floor repeat
small 2x tiles. Outfits share sprite indices with four small suit palettes.
No heap allocation, PNG decoder, full-screen buffer, decompressed texture
cache or additional static RAM was introduced.

## Build and resources

ARM GCC 15.2.1, nwlink 1.0.0, C11, -Os, LTO, warnings as errors. Standalone and
module builds pass. Release/debug installed bytes match on N0110, N0115/custom
RAM, N0120 and Upsilon. A game-local lossless icon/ELF packer is included.

| Resource | Bytes |
| --- | ---: |
| Downloadable NWA | 57,432 |
| Installed code/assets, all profiles | 49,732 |
| Static RAM, all profiles | 5,857 |
| Observed deepest ARM stack | 436 |
| Save payload / with record overhead | 260 / 281 |
| Packed launcher icon | 1,559 |
| Encoded sprite payload, palettes and row offsets | 33,954 |

The preceding procedural build was 20,008 download bytes / 13,804 installed.
Original artwork increases flash use, while static RAM and saves are unchanged.
Packed 4-bit literals and reduced palettes cut the first sprite prototype from
64,044 to 57,432 download bytes. Original source PNGs are not installed on the
calculator. Rendering still uses one 5,120-byte strip and fixed object pools.

## Native regression suite

TinyCC compiled and executed the final native suite; ARM GCC checked its source
with -Wall -Wextra -Werror. Results:

- 80/80 controller seeds reached 3,000 meters, covering 692,000 simulation ticks,
  all five hazard types, shields and four simultaneous hazards.
- Another 200,000 irregular-input ticks passed game-state validation.
- Floor, ceiling, centre-hovering and shielded-floor camping all collided by
  45 / 71 / 45 / 98 meters across 200 seeds per strategy.
- Rotating beam tests cover all 128 phase/direction values, finite segment
  collisions, safe empty corners, shield protection and phase save round trips.
- Shop tests verify exact 50/120/200 prices, permanent ownership, free
  re-equipping, rejected insufficient funds and identical outfit physics.
- JJN1 migration, exact JJN2 round trips, one-bit corruption of each save byte,
  invalid fields and 1,000-tick restored/uninterrupted equivalence pass.
- Every view and selection, 240 boundary/exhaust frames, 128 rotating-beam
  frames and 96 title-animation steps match full redraws pixel for pixel.
  Unchanged frames push no pixels. Push bounds and the 2,560-pixel strip
  capacity are asserted by the display callback.
- Animated menu renders leave simulation and statistics byte-identical.

Physics, generation, pricing, controls, save layout and persistence code were
not changed by the artwork revision.

## Compiled ARM application

The full app suite passes on N0110/Epsilon (148,928 bytes RAM allowance),
N0115/custom RAM (133,652), N0120/Epsilon (153,676) and Upsilon (107,674).
Held-key launch protection, pause, cancelled restart, resume, exact saved-world
restoration, Home/On-Off exits and preservation of unrelated records pass.
RAM cleanup, Home unlocking and firmware-RAM integrity are checked.

Shop navigation cancels/buys with real keyboard events, equips owned items,
rejects insufficient funds and preserves exact expected balances/ownership.
Legacy save import, result/restart and full-storage retry/return/explicit-exit
scenarios also pass without damaging existing records.

A live keyboard run reaches 986 meters, clears 32 hazards and collects 80 coins.
It includes rotating lasers and every hazard type. The crowded scene and the
four-rotor stress scene pass. All these new-artwork scenarios peak at 12 ms
per draw, with no draw over 32 ms; deepest observed stack is 436 bytes.

The final change makes the title background scroll every cosmetic-clock step.
Its focused ARM suite passes again on all four profiles: 16 distinct snapshots,
unchanged zero-initialized simulation, preserved sentinel storage and clean
Home exit. Title draws peak at 12 ms and stack at 412 bytes. The final native
suite was repeated after this clock-only adjustment. Gameplay screens/code
are unchanged from the preceding full ARM-suite run.

Screenshots and the animated title GIF in `docs/` come from the actual compiled
ARM app. Emulator output/reports remain in ignored `output/`. Physical-device
display timing and feel remain to be checked; emulator timing is not a physical
calculator performance claim.

The isolated publication build is byte-identical to the validated local app.
Native, lossless-packing, asset-regeneration, four-profile ARM gameplay and
four-profile animated-menu suites pass from the publication snapshot using
the upstream repository helpers.
