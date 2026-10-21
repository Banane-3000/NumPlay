# Validation — Core Ball

Validated on 21 October 2026 with GCC 15.2.1 (ARM), GCC 16.2 (host), nwlink 1.0.0 and NumPlay's Unicorn-based ARM emulator. The reference is pinned in `assets/provenance.json`.

| Check | Result |
| --- | --- |
| Original campaign | All 60 levels completed by a timing-only solver, 36,964 simulated frames; at most four attempts per level |
| Actual original JavaScript | 66,000 motion samples; worst angular error 0.00632 degrees |
| Rotation soak | 4,320,000 frames across the twelve used motion behaviours; bounded finite angles |
| Save data | Round-trip, null/truncated data rejection, every-byte corruption rejection, final 60/60 progress |
| Actual ARM app | N0110, N0115, N0120 Epsilon memory profiles and the smaller Upsilon profile pass |
| Pause | Game state remains byte-for-byte unchanged until resume |
| Exit | Back/Home/Power handling, held launch keys, execution lock release, clean app RAM and unchanged firmware RAM |
| Filesystem | Existing files retained; checked save reload; full storage supports continued play and exit |
| Ending | Last level unlocks 60/60, saves it and replays level 60; no level 61 |
| Rendering | All 60 level layouts and the dense victory scatter render safely; maximum observed ARM render time 7 ms |
| Release packing | Installed release/debug bytes are identical at the 107,674-byte RAM budget |

Text is now rasterized at its real 13/22/32-pixel sizes with four-bit antialias coverage. Menus, help, counters and the antialiased title/GO masks were inspected at the native 320 × 240 size. No heading is enlarged from a small bitmap. Physics and the existing save format are unchanged.

Release: **35,692 bytes**; installed image: **30,653 bytes**; static app RAM (`.data` + `.bss`): **5,557 bytes**; deepest measured stack: **388 bytes**. No heap is used. The icon is kept in flash and is not counted as static RAM.

SHA-256: `43c1b6d9be3c1d554193d9465cb5c1faa60406498092830b17cb7b371ab359f4`.

Machine-readable results are retained in [docs/validation.json](docs/validation.json). Screenshots in `docs/` come from the tested host renderer or actual ARM app. ARM timings are emulator measurements, not hardware benchmarks; physical LCD transfer and firmware timing can differ. N0115 testing uses its memory budget with the emulator's compatible N0110 processor profile. Hardware playtesting remains the final check for perceived input and display smoothness.

The published artifact is built against the upstream common safety helpers at `979c19e828fee928d2d9b2843f8a2f6f2fea500e`.
