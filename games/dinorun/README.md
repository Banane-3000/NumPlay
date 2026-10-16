# Chrome Dino — NumWorks native C sprite port (prototype)

This C game uses the original *rectangle-defined pixel art* decoded from [Lily's Python Chrome Dino version](https://github.com/LilyTheCuteCatgirl/lilys-small-numworks-games/blob/main/games/chrome_dino.py). The jump/duck/flying frames, cacti, outline clouds, stars, moon and ground details are now copied precisely as rectangles rather than hand-redrawn placeholders. The existing C gameplay/NumWorks integration remains independent. Unofficial, not affiliated with Google.

**Art rights warning:** The source Python script credits **Diego KHELLADI** (2026) and displays an **all-rights-reserved** statement; Lily's collection also describes third-party contributions. These sprites are included for a private prototype only. **Confirm permission from the original rightsholder before pushing the sprite-based port as an upstream/public PR or distributing it.** Attribution alone does not establish permission.

![Gameplay screenshot](docs/shot.png)

## Controls

| Key | Action |
|---|---|
| **OK**, EXE, Up / 8 | Jump / start / restart |
| **Down** / 2 (hold) | Duck (and fall faster mid-air) |
| **Back** or Backspace | Pause / resume |
| **SHIFT** at title | Help screen |
| **HOME** | Exit back to calculator |

Physics now follows the Python original: **49-pixel jump apex**, 10–14 px/frame cactus speed, 5–14 px/frame bird speed, score advancing by one each frame, and night/day switching every 300 points. Actual real-time speed can still vary with the calculator and rendering cost.

The game features the original pixel cacti, flying bird frames, scrolling clouds, moon/stars, gradually increasing speed, day/night changes, high-score storage (`dinorun.sav`) and instant restart.

## Build as an individual NumWorks app

Requires the same EADK GCC + `nwlink` environment as the rest of NumPlay.

```sh
make -C games/dinorun check
```

Result: `games/dinorun/output/dinorun.nwa`.

You can also use `.github/workflows/dinorun-quick.yml` without installing build tools on Windows. It triggers on a push to a branch with changes in `games/dinorun/**`, runs a simple host test, builds the app and uploads **DinoRun-nwa**.

Install the `.nwa` using https://my.numworks.com/apps (Chrome/Edge). **NumPlay is already close to the calculator's entire app-space budget, so installing Dino Run separately may replace your existing NumPlay installation. Back up scripts and saves and reinstall NumPlay afterward.**

## Integration into the combined NumPlay launcher

`module` and `sim-module` targets are already included to support future integration. For a full English NumPlay hardware test, run `python3 tools/integrate_dinorun.py` at repo root and then push; the `Dino inside NumPlay (full launcher build)` workflow triggers automatically on `add-chrome-dino`. The integration reclaims flash by lowering the color depth of five carousel previews while preserving all games. See `FULL_NUMPLAY_README.md`.

## Development checks

```sh
make -C games/dinorun host-test
```

Host tests cover jump motion, ducking, collision, game over, restart, and rendering. They do **not** replace on-calculator testing or NumPlay's ARM emulator suite.

The game is made as an independently coded fan-style runner, with a deliberately small screen buffer (16 RGB565 rows) to avoid allocating a full 320x240 framebuffer.
