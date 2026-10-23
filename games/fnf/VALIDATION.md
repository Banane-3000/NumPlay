# Validation — 23 October 2026

This is a native, silent calculator adaptation. The chart content is complete for the pinned assets; desktop presentation is adapted as described in README.md. No physical calculator was connected for these checks.

## Build and limits

| Measurement | Result |
| --- | ---: |
| Released `.nwa` | 1,840,140 bytes |
| Installed flash bytes | 1,829,516 bytes |
| Shared flash limit | 2,031,616 bytes |
| Static app RAM | 23,396 bytes |
| Highest measured ARM stack after final input change | 2,608 bytes |
| Minimum tested installer RAM allocation | 107,674 bytes |
| Background cache | 9,600 bytes |
| LCD strip | 5,120 bytes |
| Game state including records and notes | 2,352 bytes |
| Save payload | 1,344 bytes |

Release SHA-256: `a3a90cd6895cb820578e9a7a5a71813c8d4d16cacb17244881e839c689ad3a43`.

Both the Python build and `make check` passed the real nwlink conversion with the minimum RAM allocation. Release and debug packages produce **identical installed bytes**. Generated ROM C tables, IDs and the chart manifest reproduce byte for byte from the checked-in packed inputs.

## Chart and engine checks

- 27 songs, 61 variations, 166 selectable charts, 90,437 notes, 21 stage backgrounds and 46 character configurations.
- Every note's direction, kind, head time and hold duration were compared against the preserved source JSON. Maximum time/duration quantization error: 0.03125 ms.
- All 166 charts were completed with a host-side input oracle using the actual native engine. Every scoreable player head received Sick; no missed player heads, queue overflow or forced completion. Opponent notes and chart events were processed during these runs.
- The worst observed working queue was 47 notes. A separate conservative interval sweep, retaining missed holds through their entire duration, bounds it at 49 including the one-note lookahead, below the fixed 64 slots.
- All 166 charts also fail normally with no input. Random inputs were checked for bounded note count and health; restart resets per-run state.
- Judgement boundaries, the symmetric/monotonic PBOT1 curve, empty taps, ghost tapping, same-lane key aliases, long-note rewards, early hold release penalties and the 160 ms tail grace were tested.
- Save round trips, inert backup recovery, rejection of every individual flipped save bit, truncated saves and rejection without changing the existing progress were tested.

## Rendering and input

Native font text has measured pixel bounds; it is not taken from a scaled browser screenshot or a large text atlas. Menu choices, every freeplay song/variation/difficulty, every story group and each referenced stage were rendered through the actual eight-line strip renderer. Screenshots were inspected, including the logo, health HUD, pixel characters and the six-character collaboration scene. Adobe clipping masks were corrected during asset conversion.

A regression test verifies **30 keypad polls per frame even when all 30 LCD strips are unchanged and no LCD push is needed**. Rendering reads a fixed game snapshot; timestamped key transitions are consumed afterward. The chart clock never advances according to frame count.

The final ARM smoke run passed all four memory profiles and rechecks both key layouts after this input fix and the final logo sizing. The 16-note real-key sequences received 16 Sick judgements on every profile; the measured gameplay sampling interval was at most 2 ms in those runs. Ordinary Bopeebo frame work was at most 13 ms on N0120 and 31 ms on the older processor profile. Dense/crowded rendering fixtures reached 16 ms / 41 ms respectively, including a fixture-only background cache switch. These are instruction-driven emulator estimates, not hardware frame-rate guarantees. Exact per-profile measurements are in `output/arm/smoke-report.json`.

The earlier complete Bopeebo run used 51 real player note presses and cleared the song. After the final input and logo adjustments, the complete 166-chart host regression, real-key smoke runs, pause/retry, persistence, full-storage checks, firmware guards and both heavy rendering fixtures passed again. Static RAM plus the highest measured stack use totals **26,004 bytes**.

## ARM / firmware checks

The compiled Cortex-M app ran against Epsilon N0120, Epsilon N0110, the smaller N0115 RAM profile and Upsilon memory layouts. The test drives the real menus and keys. It checks:

- Arrow controls and 7 / 8 / ( / ) controls, including simultaneous aliases for one lane.
- Song selection, accurate notes, frozen pause clock, resume and retry countdown.
- Home and Power exits, save/backup round trips and recovery when only the Python backup remains.
- Full storage: failed-save message, retry, return to menu and explicit unsaved exit; unrelated records remain intact.
- A held confirm key at launch does not enter a menu accidentally.
- Firmware RAM guards, clean application RAM on exit and balanced Home locks; Upsilon makes no Epsilon-only system calls.

Host-engine snapshots explicitly exercise the dense 2hot chart and the collaboration's larger visible cast on both processor profiles. These fixtures test rendering, input sampling and safe exits; they are not presented as real-key song completions.

CPU emulation uses 216 million instructions/second for the N0120 profile and 86.4 million for the older processor profile to preserve their relative speed. It does not model LCD transfer cost or every cache/pipeline effect. Physical play testing remains necessary to confirm frame pacing on a particular firmware and calculator.

## Reports and scope

Reproducible commands are in README.md. Reports and screenshots are generated under ignored `output/`: `build-report.json`, `asset-report.json`, `host-report.json`, `autoplay-report.json`, `arm/report.json` and `arm/smoke-report.json`.

This contribution creates a standalone `.nwa`, registers its build and release output, adds a big-game download card and credits, and provides focused CI. It leaves the launcher manifest and all other game directories unchanged. The source art/assets' redistribution restrictions remain in force; see ASSETS.md and the original license texts.
