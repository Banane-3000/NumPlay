# Validation — 23 October 2026

This updates the existing `twoplayer` module and `TwoPlayers.nwa`. There are now **15 games**: the original four plus eleven additions. Turtle Hop is removed. Player 1 uses **OK** for Action; Player 2 still uses **EXE**. No physical calculator was connected for these checks.

## Build and footprint

| Measurement | Result |
| --- | ---: |
| Release `.nwa` file | 93,728 bytes |
| Installed flash | 53,912 bytes |
| Standalone static RAM, including installer alignment | 22,065 bytes |
| Highest measured standalone ARM stack in these runs | 580 bytes |
| Module code and read-only data | 33,597 bytes |
| Module shared arena RAM | 22,052 bytes |
| Working state shared by the eleven additions | 352 bytes |
| Save payload | 128 bytes |
| Smallest tested installer RAM allocation | 107,674 bytes |

Release SHA-256: `c4dba2cd62da31fe9ff72cdf13045b976019fedecc8dc741e32f2ebc659bd036`.

ARM GCC passes with `-Wall -Wextra`. The release and debug packages convert with real nwlink at the smallest RAM allocation and produce identical installed bytes. The root Makefile's actual module and `build/data/apps/TwoPlayers.nwa` targets pass; the release copy matches the tested package.

There are no imported textures, sounds or heap allocations for the mini-games. Artwork uses the existing shared shapes/fonts, fixed projectile pools and LCD strips. Physics stays on the fixed eight-millisecond step with bounded catch-up. The new bot intention cache is shared, rather than allocating one AI engine per game.

## Gameplay and difficulty

All fifteen bots were reviewed. Pong/Hockey use slower, sampled targeting with aiming error. Tanks reacts and reloads more slowly, moves more slowly, and has less accurate bank shots. Light Cycles has delayed decisions and limited lookahead, with no prediction of the player's future cells. The additions receive longer reaction times, reduced movement/attack rates, imperfect archery and throwing accuracy, and slower/error-prone memory responses. Two-human movement and physics do not get these bot handicaps.

- **33 complete matches** cover the eleven additions at all three difficulties against an idle human. CPU decisions are real; no score injection or forced completion is used. This checks progress, not human difficulty balance.
- **150 seeded random-input runs** cover all fifteen games and opponent modes, checking score bounds and finite state.
- **120 additional scripted-player matches** exercise Quick Draw and Archery, twenty per difficulty per game. The 420 ms reaction policy wins Quick Draw in [20, 20, 16] matches at Easy/Normal/Hard. The 650 ms charge / 1.8 s shot-cycle policy wins Archery in [20, 20, 20] matches. These reproducible policies show the bots are beatable; they are not human play-test results.
- Tug of War is won against each bot with five presses per second. Pong and Football explicitly wait before reacting. A solo EXE action cannot also trigger the bot.
- Targeted regression checks cover false starts/ties, tap debouncing, goals/rebounds, volley net-top collisions, charged arrows/hits, dodge-ball knockouts, mirrored obstacles, rocket bounds, sumo, fishing hold/reel/contested catches and memory watch/input/error/tie rules.

## Readability and controls

Archery is rebuilt with visible bows, directional arrows, player-coloured trajectory dots, angle values, power bars, hit flashes and three hearts. The trajectory uses the same launch/gravity formula as the real arrow, with the small integration difference of an eight-millisecond tick. The tutorial explicitly explains charging/releasing and the new OK/EXE layout.

The review keeps the established layouts where they already communicate the rules. Selected improvements add Football's clock, Dodge Ball's hearts, Rocket Race's boost flame and Fishing's hold/release prompts. Tanks' rules distinguish driving and turning. Menu pagination handles a last page containing three games, including page wrapping from its final row.

All fifteen rules/game screens, four menu pages, pause/results/failure cards are rendered. English, French and Chinese run the same native engine and text-height checks with the real Nunito/Fusion Pixel fonts. Representative screenshots and actual ARM Archery charging/flight frames were visually inspected.

## Saves and actual ARM checks

The original 36-byte v1 save migrates with all sixteen original win counters preserved. The 136-byte v2 format migrates to 128-byte v3: the removed game's counters are dropped, Memory Duel's counters move intact to its new ID, and selecting the removed game falls back to Pong. Old Memory selection restores Memory correctly. Both v2 cases also run through the installed ARM package. V3 round trips, truncation and every individual flipped save bit are tested without mutating progress on rejection.

The compiled app passes N0120, N0110, the smaller N0115 allocation and Upsilon profiles. Each opens all fifteen games through real keys and checks pause/resume/restart, selection and page wrapping. Real OK and EXE presses score independently in Quick Draw. In Archery, both keys charge and release independently; Shift does nothing, and real arrows remain in flight after release. Home/Power saves, relaunch, held launch keys and full-storage retry/explicit unsaved exit are checked. Unrelated records stay intact.

Memory guards check unaligned storage accesses, flash writes, firmware RAM preservation, cleaned app RAM on exit and balanced Home locks. Upsilon makes no unsupported Epsilon-only system calls.

`tests/test_integration.py` compiles the actual launcher and module pipeline with a deliberately one-module **test catalogue**. N0120 and Upsilon open 2 Players, start a game, return to the carousel, re-enter and exit safely. The real record and NumPlay's Python backup are verified. Maximum measured launcher/integration stack is 1,056 bytes. This is not a full release build of every unrelated NumPlay game.

Reports/screenshots are generated under ignored `output/`: `build-report.json`, `host-report.json`, `balance-report.json`, `arm/report.json`, `integration/report.json` and `languages/`. Commands are in [README.md](README.md). Physical LCD/key-matrix behaviour and subjective difficulty still need play testing on a calculator.
