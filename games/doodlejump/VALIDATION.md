# Local validation — 20 October 2026

Standalone Doodle Jump with bounded movement, a controlled-random route and
an English-only interface. Publication changes are limited to this game and
its link/credit in the root README.

## Boundaries and reachable routes

The player centre is clamped to 18..270 within the 288-pixel playfield, keeping
the character on screen. An outward horizontal velocity becomes zero at the
wall; an opposite input accelerates inward immediately. Landing interpolation
uses the actual clamped displacement. Collision distances are direct, with no
opposite-edge contact. The renderer no longer draws wrapped copies. Moving
platforms and monster patrols also remain within the page.

The route now samples nearly vertical, short and larger offsets rather than
following bands in order. A random direction can reverse away from the walls;
long direction runs are limited, and four consecutive ledges in one half
trigger an inward bias. Reflected proposals avoid edge clamping/pileups.
One existing history byte tracks the same-half count, direction and run length.

A generation-only probe checked 28,800 ledges over 120 seeds. Direct base-centre
gaps were at most 68 pixels; no same-half streak exceeded six. Each seed had
both short and long hops, repeated movement in the same direction and over ten
reversals away from the edges. Across the sample there were 12,045 such interior
reversals. Counts in six left-to-right zones were 849 / 4,311 / 9,213 / 9,275 /
4,286 / 866. This is a controlled, centre-biased distribution with no fixed
visitation schedule, not an equal-count cyclic route. Every seed reached both
halves and all four inner zones; the aggregate reached the whole route range.
Moving platforms and the player stayed within the playfield.

The normal steering/shooting controller completed 6,000 height on 75 of 80
seeds. Four runs ended from monster contact and one from a fall. Highest climb:
6,007. All platform types appeared. Seed 1 reached 6,001 with 51 bounces,
13 springs, two rockets and two monsters cleared. The test requires broad
successful play (at least 70/80); these observed losses do not assert that a
human cannot fall or hit a monster. Another 120,000 irregular-input ticks
passed state validation. The controller is test code, absent from the app.

## Saves and regression tests

DJN3 remains 384 bytes and uses the existing memory layout. The decoder still
accepts checked DJN1 and DJN2 records, preserving lifetime records, height, camera,
peak, items and random state. It clamps an imported edge position, places the
nearest supporting route ledge under the character and limits gaps in the
route above it before continuing with the wall-aware generator. This one-time
layout adjustment prevents an old queued wrapping jump from blocking progress.
The original legacy fixture and an actual wrapping climb were tested. An
actual DJN2 bounded climb imported byte-identical world/record data, with only
its old generator phase reset. New DJN3 saves retain random-generator history,
so subsequent reloads preserve exact state and future route choices.

Tests cover sustained wall contact, immediate reversal, maximum displacement
per tick, no landing across the old seam, clamped moving ledges, descending
landings, pass-through while rising, platform types, centred/off-centre boosts,
shots, stomps, fatal monster contact, rocket protection and death-state freeze.
Save corruption, invalid values and repeated reloads remain covered. Partial
and complete redraws matched pixel for pixel; unchanged screens pushed no data.
The native suite ran with TinyCC, and ARM GCC checked the test source with
warnings treated as errors.

## Actual ARM program

N0110/Epsilon, N0115/custom RAM, N0120/Epsilon and N0110/Upsilon passed the
compiled-app suite, including held fire, pause/cancel/resume, saves, Home and
On-Off exit, RAM cleanup, Home locking, unchanged firmware RAM and preservation
of unrelated calculator records. Each profile also ran separate held-left and
held-right flights: sampled positions stayed bounded, moved continuously,
reached the expected wall and had zero outward speed there.

A keyboard-controlled run reached 1,202 height, with 12 bounces in 650 ticks.
Crowded rocket rendering, a generated saved climb, reduced animations and
full-storage handling also passed. Maximum measured render time was 5 ms,
with no frame over the 32 ms drawing interval. These are emulator results;
physical calculator timing remains to be checked.

## Build and resources

ARM GCC 15.2.1, C11, `-Os`, LTO, nwlink 1.0.0 and a game-local lossless packer.
Both standalone and module builds completed. Release/debug installed bytes
match on all four profiles.

| Resource | Bytes |
| --- | ---: |
| Standalone NWA | 19,764 |
| Installed code/assets | 13,664 |
| Static RAM | 6,065 |
| Observed ARM stack | 548 |
| Save payload | 384 |
| Compressed icon | 409 |

The preceding wall build used 19,524 download bytes and 13,436 installed bytes.
The controlled-random update adds 240 download bytes and 228 installed bytes;
static RAM, stack observation and the save payload are unchanged. There is no
heap allocation or full framebuffer. Actual ARM screenshots are in docs;
build/validation reports are in the ignored output folder.

The isolated publication build matches the locally validated app byte for byte.
Host, packing and four-profile ARM suites are repeated from this snapshot using
the upstream repository helpers.
