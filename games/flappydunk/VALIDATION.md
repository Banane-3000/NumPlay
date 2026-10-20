# Local validation — 20 October 2026

Simplified-menu/endless-only update. ARM GCC 15.2.1, nwlink 1.0.0, C11,
`-Os`, LTO and warnings treated as errors.

| Resource | Previous version | Updated version |
| --- | ---: | ---: |
| Standalone NWA | 26,472 bytes | 23,580 bytes |
| Installed flash | 19,652 bytes | 17,532 bytes |
| Static RAM | 5,965 bytes | 5,941 bytes |
| Observed standalone ARM stack | 696 bytes | 648 bytes |
| Save payload | 160 bytes | 160 bytes |

The downloadable app is 10.9% smaller; installed flash is 10.8% smaller.
Challenge data, mode-specific physics, the selection screen and medal UI are
removed. The existing lossless icon/ELF packing remains. Release/debug apps
produce identical installed bytes on N0110, N0115/custom, N0120 and Upsilon.

- Host suite: scoring direction and complete passage, rim collision, missed
  hoops, floor/ceiling, saturating totals, 120,000 endless simulation ticks,
  save round trips every 19 ticks and every single-byte corruption rejected.
- Score-only unlocks: each locked/exact boundary at 15, 35, 70, 120 and 200,
  permanent ownership and an actual 13-to-15-point scoring transition pass.
- Both legacy fixtures were generated and validated with the old engine.
  Normal-flight import preserves flight state, totals and previously unlocked
  balls; a discontinued course becomes a fresh endless flight. New `FDK2`
  saves have zeroed old mode/medal fields and retain cosmetic ownership.
  Malformed-but-checksummed data leaves live state unchanged.
- Partial redraws match full redraws pixel-for-pixel over 140 moving frames;
  unchanged frames produce no display push. No push exceeds 320 x 8 pixels.
- Offline reachability controller: 50 baskets, 16 swishes, 70 points and 174
  flaps through the normal input API, over 89.4 simulated seconds. It is a
  test-only controller and is not part of the app.
- Actual ARM app on all four profiles: held launcher input ignored, one flap
  per fresh press, pause freeze, ready-on-resume, save/reload, preservation of
  unrelated files and safe Home/On-Off exits. Maximum observed draw time:
  15 ms; no draws exceeded the 32 ms presentation interval.
- ARM input replay scores at least three baskets and one swish, with live
  particles and multipliers; maximum observed draw time 14 ms.
- ARM menu navigation exercises the three title choices, Locker's locked
  Cosmic ball at 200 points, guide, wraparound navigation and confirmed fresh
  flight. Locked selection does not equip the ball. Both old save fixtures
  migrate on the actual ARM app, retaining the selected Cosmic ball.
- Full-storage exit remains interactive and existing data survives. Explicit
  unsaved exit works. App RAM is cleared and firmware RAM remains intact.
- Repository standalone smoke test passes two sessions, including a record
  inserted after the first save. The published app does not register a game
  in the combined launcher or alter its build, workflow or release scripts.
- The lossless packing tests and four-profile ARM scenarios are repeated from
  the isolated publication snapshot using the upstream repository helpers.


Captures in `docs/title.png`, `docs/gameplay.png`, `docs/locker.png` come from the actual ARM app. Timings use the repository
emulator model; they are not physical-calculator FPS measurements. The full
all-game/translated build and physical NumWorks testing were not repeated.
