# UNO validation

Validated locally on 23 October 2026. These are automated host and ARM-emulator
results, **not physical-calculator test results**.

## Build and memory

ARM GCC 15.2.1, `-Os`/LTO, nwlink 1.0.0. Release and debug NWAs produce identical
installed bytes for every tested profile. The standalone game and a launcher
module both compile. The catalogue, standalone export, credits and build tests
add UNO only; the other games are unchanged.

| Measurement | Bytes |
| --- | ---: |
| Distributable `UNO.nwa` | 129,120 |
| Installed code, metadata and art | 103,624 |
| Static RAM, including LCD strip | 11,208 |
| Maximum stack observed during ARM scenarios | 1,040 |
| Save record payload | 768 |
| All 55 card sprites, palette and offsets | 60,895 |
| Table texture | 1,536 |

Installed size comes from nwlink's actual calculator link, not ELF file size.
Static RAM is `_heap_start - _data_section_start_ram` in the installed ELF.
The observed stack maximum is a scenario measurement, not a mathematical bound.
No game code allocates heap memory.

| ARM profile | Available application RAM | Result |
| --- | ---: | --- |
| N0120, Epsilon | 153,676 | Passed |
| N0110, Epsilon | 148,928 | Passed |
| N0115 RAM profile, Epsilon | 133,652 | Passed |
| N0110/N0115, Upsilon | 107,674 | Passed |

The N0115 test uses the older-model CPU/memory layout with its configured RAM
limit; it is not an independently emulated physical N0115 board. Emulator work
estimates average approximately 14 ms per redraw on the N0120 profile and 36 ms
on the older profiles. These are not measured display refresh rates on hardware.
The app only redraws when its visible state changes.

## Rules engine

`tools/test.py` builds and runs the C tests with the host compiler. Tests cover:

- Color, number and action matching; legal Wilds and bluffable +4 cards.
- Two-player Reverse/Skip and multiplayer direction changes.
- One-card draws, voluntary draws, keeping a drawn card, and forbidding play of
  a different hand card after drawing.
- Legal and illegal +4 challenges, offender/target penalties and skipped turns.
- Direct victory on an empty hand for humans and bots, including when an
  opponent holds only a zero card; last-card +2/+4 resolution before victory.
- UNO pre-call, late self-call, catching, immunity after the next action and
  automatic UNO.
- Same-kind penalty stacking, no mixing, no challenge for stacked +4, and no
  stacking after going out; draw-until-playable and forced drawn play.
- Exhausted-pile recycling while preserving the top discard and every card.
- 12,000 initial deals checking opening actions and deck composition.
- **1,152 complete single games**, spanning all 32 house-rule combinations,
  2/3/4 players, all three bot difficulties and four seeds per combination.
  The final run used 83,450 actions, with no blocked-table redeals. Every step
  checked the exact 108-card multiset; the largest simulated hand had 39 cards.
- Save round trips, truncated/invalid saves, semantic state rejection and every
  individual bit flip across the entire 768-byte save, without modifying the
  live state when a save is invalid.
- A real version-1 save with a 500-point target and accumulated totals migrates
  to version 2 with the cards intact and former totals/targets zeroed. A won v1
  round migrates to a finished game. Point fields and calculations are removed
  from the live rules engine; the reserved wire slots preserve compatibility.

## UI and calculator execution

The host tests validate every RLE row's bounds, real-font help wrapping, large
hands and scrolling, and render screenshots of the menus, table, settings,
variants, instructions, color selection, challenge, evidence, table rules and result.

The visual table setup has separate Players, Rules and Ready steps, with visible
seats, five on-screen switches and a final summary. Host tests navigate each
step, every switch, the classic reset and returning to earlier steps, verifying
that editing preferences does not mutate an existing match. Text-width checks
cover switch labels, descriptions, card names, action shortcuts and the gap
between the pile labels and active-color label. Table screenshots cover two,
three and four players, bot turns and a large hand. Browsing during bot turns
and skipping unavailable action buttons are exercised too.

`tests/test_arm.py` runs the **actual ARM application**, including Epsilon/Upsilon
system calls. Each profile exercises menu navigation, bot turns, the real key
mapping, pausing, all help pages, table rules, save-to-menu, new-game replacement
cancellation, continue, Home/Power exits and reload.
The setup scenarios select player counts, enable +2/+4 stacking, restore classic
rules, review the table and return through all setup steps after canceling a
replacement. Setup contains no point target; the HUD, pause menu, help and
result screen contain no scores. Play-again uses the completed game's rules,
even when the saved preferences for a future game differ.

Additional engine-validated saved positions exercise actual keys for playing,
calling UNO, drawing, rejecting an old hand card after drawing, keeping the new
card, choosing a Wild color, winning and both legal/illegal +4 challenges with
the hand-reveal screen. A completed game records its win once across reloads.
Actual key tests also verify Play again and migration of the real v1 save with
its old totals removed and cards preserved.

Root Make targets `build/data/apps/UNO.nwa` and `build/data/modules/uno.o`
pass, including a parallel request for both outputs. The module occupies
81,776 bytes of code/read-only data, 4 bytes of initialized data and 11,188 bytes
of BSS in the launcher's shared arena. Root standalone output matches the
game's release byte for byte.

`tests/test_integration.py` builds the real current launcher with a deliberately
UNO-only test catalogue, using the production catalogue entry and generators.
On N0120/Epsilon and N0110/Upsilon, real keys launch UNO, navigate its three-step
setup, play, save, return to the carousel and re-enter the game. Home exits the
launcher cleanly. The shared `numplay_saves.py` contains the game record;
retaining only Python scripts simulates reinstalling and restores that record
exactly. The maximum observed stack in this launcher scenario is 1,064 bytes.
This focused test is not a complete build or playthrough of all existing games.
The tests also cover a held launch key, a corrupt save and full storage with
retry and explicit exit without saving. An unrelated Python record is preserved.

Memory guards report no invalid accesses, firmware RAM changes or unintended
flash writes in these scenarios. Exit releases the Home lock where supported
and clears application RAM before returning to firmware. Upsilon makes none of
the Epsilon-only system calls.

Reports are reproducible in `output/sizes.json` and `output/arm/report.json`.
The screenshots in `docs/` are generated from these tests. Hardware testing is
still needed to confirm firmware-version-specific installation and LCD timing.
