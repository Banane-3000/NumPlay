# Donkey Kong validation

Verified locally on 2026-10-21, including an isolated publication snapshot
based on upstream `09943ea31e9198816ead6e33c75788cda6810048`.

## Footprint

| Item | Measured size |
| --- | ---: |
| Release `.nwa` transport file | 21,912 bytes |
| Installed flash image | 15,824 bytes |
| Static RAM (`.bss`) | 6,209 bytes |
| Maximum stack observed in ARM checks | 316 bytes |
| Game state, including all entity pools | 864 bytes |
| LCD drawing strip | 5,120 bytes |
| Record save payload | 32 bytes |
| Packed launcher icon | 346 bytes |

The renderer draws directly from packed 4-bit sprites into one eight-row strip.
Strip hashes avoid sending unchanged rows. There is no heap, full framebuffer,
floating-point physics, external asset pack, or ROM. Fixed 60 Hz simulation
uses a bounded catch-up loop and does not advance while paused.

## Tests

- C11 host build with `-O2 -Wall -Wextra -Werror`.
- Actual walking, jumping and climbing routes through all four boards;
  obstacles and items are disabled for those geometry checks. The elevator
  route boards a moving lift and transfers to the right-hand girders.
- Separate live obstacle checks for barrel jumping, one-time jump awards,
  hammer hits, body collisions and capped scores/extra lives.
- Broken/retracting ladders, hammer restrictions, held jump input, fatal
  falls, timeouts, life loss, game over, rivet holes and round progression.
- 48 seeded random-input simulations (up to 18,000 steps each), checking
  entity bounds and invariants at every step.
- Save round-trip, every-byte corruption, truncated/null input, preserving
  the prior decoded record on failure.
- Host and actual ARM screenshots of the boards, title, pause, instructions
  and storage-full dialog; inspected visually.
- Actual ARM app on Epsilon N0120 (153,676-byte app RAM), Epsilon N0110
  (148,928-byte app RAM) and Upsilon (107,674-byte app RAM): launch, pause,
  resume, save, reload records, Home/On-Off exit.
- Full filesystem: keep playing, retry exit, and leave without saving;
  unrelated files remain byte-identical.
- Exit checks: no invalid accesses, firmware RAM changes, unbalanced Home
  locks, or uncleared app RAM.
- Release and debug artifacts install to identical bytes on the smallest
  RAM profile.
- Publication builds with the current upstream Home/exit and file helpers;
  the exit helper waits for held keys to be released before returning Home.
- `make build check test` passes from the isolated snapshot. Release
  generation lists `DonkeyKong.nwa` once, with Bobydeluxe's credit. The
  combined launcher manifest is unchanged, and no other game is modified.

The four-board ARM render sample recorded 193 frames, a slowest draw of
11 ms, and no draws over 17 ms at the emulator's 216 MHz instruction budget.
This is an emulator measurement, not a physical-device benchmark. There has
been no physical-calculator playtest or exhaustive human playthrough of every
difficulty level. N0115 shares the N0110 architecture but was not separately
benchmarked here.

Reproducible local reports: `output/build-report.json` and
`output/arm/report.json`. Outputs and toolchain binaries are ignored by Git.
