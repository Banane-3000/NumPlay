# Validation

Local classic-style update, 2026-10-20. ARM GCC 15.2.1, nwlink 1.0.0.

| Metric | Previous app | Updated app |
| --- | ---: | ---: |
| Standalone `.nwa` | 31,324 bytes | 20,880 bytes |
| Installed flash | 17,520 bytes | 15,600 bytes |
| Static RAM (`.data` + `.bss`) | 7,245 bytes | 7,225 bytes |
| ARM stack high-water mark in tested scenarios | 372 bytes | 376 bytes |
| Save payload | 608 bytes | 608 bytes |

File size decreases by 33.3%; installed flash decreases by 11.0%. The update
removes reserve/queue/hint/combo logic and uses a game-local lossless packer.
Packing alone adds no runtime memory or decoding work. The updated visuals,
geometry and random rules are intentional gameplay changes. Save-record
headers/name overhead is additional to the payload above.

- Host C suite: 28,800 physics ticks across 12 seeds, save round trips every
  17 ticks, every single-byte save corruption, invalid-but-checksummed payload,
  coincident contacts, watermelon clearing, overflow/recovery, blocked spawn
  and base-only consecutive merge scoring. A 100,000-drop deterministic
  generator test checks all five ranks, approximate frequency balance and
  repeated streaks. Old `SKA1` fixture import and stable `SKA2` round trips pass.
- Dirty and full rendering match pixel-for-pixel over 120 changing frames,
  with additional edge/cloud/cooldown, five next-fruit and maximum-score cases.
  No display call exceeds the 320 x 8-pixel staging buffer.
- Actual ARM application: pause freezes simulation; resuming does not catch
  up paused time. Save/reload, Home/On-Off exit, cleared app RAM, unrelated
  file preservation and balanced Home locking pass on N0110/Epsilon,
  N0115 custom (133,652 bytes), N0120/Epsilon and Upsilon (107,674 bytes).
- Dense 48-fruit ARM basket: 76 presentation cycles over 2.5 simulated seconds,
  maximum instrumented draw time 9 ms and no draws over 32 ms. This uses the
  repository emulator's timing model; it is not a physical-device FPS test.
- Full storage: failed exit remains visible, allows returning to play and
  explicit unsaved exit, and leaves the existing file intact.
- ARM legacy import preserves score, best, RNG, ranks, current and next;
  removed reserve/queue/combo bytes are zeroed when writing `SKA2`.
  Gameplay keys 1/2 leave the dropper and preview unchanged.
- Release/debug NWA files install identical bytes on all four profiles.
- Repository `test_games.py --game suika` passes both release-app sessions,
  including a file inserted after the first save.
- Real launcher/module integration, filtered to Suika: module relocation,
  generated game table and screenshots compile and link at 107,674 / 133,652 /
  148,928 bytes of RAM. Two ARM launcher sessions pass launch/save/reload,
  preserve unrelated files, write `numplay_saves.py`, clear app RAM and exit
  safely. Launcher stack high-water mark: 984 bytes. A fresh catalogue capture
  confirms the Suika card and Bobydeluxe credit.
- Captures in `docs/shot.png`, `docs/title.png` and `docs/menu.png` come from
  the actual ARM app/launcher.


The complete all-game/translated NumPlay builds and physical NumWorks
testing have not been repeated for this revision.
