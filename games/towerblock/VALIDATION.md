# Local validation — 20 October 2026

Practice removal: one normal mode and a three-entry title menu (Build/Resume,
Studio, Guide).

| Resource | Bytes |
| --- | ---: |
| Standalone NWA | 20,160 |
| Installed flash | 14,248 |
| Static RAM | 6,141 |
| Deepest observed standalone ARM stack | 612 |
| Save payload | 384 |

Built with ARM GCC 15.2.1, nwlink 1.0.0, C11, `-Os`, LTO and warnings treated as
errors. Release/debug builds install identical bytes on N0110, N0115/custom
RAM, N0120 and Upsilon. Static RAM and installed size match on all profiles.

- Old-save fixtures were generated and validated with the previous engine.
  `TBK1` normal runs resume under `TBK2`; old Practice runs reset to a fresh
  tower. An old Practice height/record of 20 leaves the normal best of 5
  untouched. A separate zero-best case confirms it cannot create a personal
  record either. Totals, attempts and preferences remain intact; deprecated
  mode/Practice-record fields are zero in new saves.
- Host tests: geometry, minimum overlap, perfect restoration, complete misses,
  frozen game-over state, every single-byte corruption, malformed-but-valid
  checksums, save round trips and migration without changing live state on
  failure. Normal input reaches 3,000 floors in 146,760 ticks with a bounded
  24-floor history. Random input adds 120,000 ticks across 40 runs.
- Dirty/full rendering matches pixel-for-pixel over 1,360 moving frames across
  four palettes. Unchanged frames push no pixels; every staging buffer stays
  within 320 x 8 RGB565 pixels.
- Actual ARM app on all four profiles: old normal/Practice save import,
  personal-best protection, three-entry title navigation, Studio/Guide access,
  normal resume, restart cancellation, fresh/held input, pause freeze,
  persistence, unrelated-file preservation and safe Home/On-Off exits.
  Observed maximum draw time in input/save checks: 6 ms.
- Scripted ARM keyboard play builds 12 floors; maximum observed draw time
  10 ms. A loaded 3,000-floor tower exercises maximum history, camera and
  fragments; maximum observed draw time 13 ms. No draw exceeds 32 ms.
- Studio settings persist. Full-storage failures remain interactive and allow
  an explicit unsaved exit while preserving existing files. App RAM is cleared,
  firmware RAM is intact and Home locking is balanced on exit.

The isolated publication build is byte-identical to the locally validated
app; its packing and four-profile ARM suites are repeated using the upstream
repository helpers. Packaging tools are local to this game.

Captures in `docs/title.png` and `docs/guide.png` come from the actual ARM app.
Timings use the repository emulator model, not physical-device FPS. The full
all-game/translated NumPlay build and a physical NumWorks playtest have not
been repeated for this revision.
