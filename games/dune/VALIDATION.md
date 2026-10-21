# Validation — Dune

Validated on 21 October 2026 with GCC 15.2.1 (ARM), GCC 16.2 (host), nwlink 1.0.0 and NumPlay's Unicorn-based ARM emulator. The supplied Dune Rider script is pinned in `assets/provenance.json`.

| Check | Result |
| --- | --- |
| Actual original JavaScript | 100 terrain curves/contact meshes match within 0.000164 world pixels; 1,000 original input cases match |
| Playability | 120 deterministic terrain seeds, 73,448 simulated frames; jumps, uphill crash conditions, score and coins all exercised |
| Recorded extremes | Best bot score 50, 90 coins in one run, 70 recycled segments; maximum surviving run 2,175 frames |
| Endless state | Bounded terrain/coin pools; coordinate rebasing retains valid world state |
| Shop | Exactly seven balls, original prices, ownership, free equip and insufficient-funds rules checked; Mountains is the only landscape |
| Actual ARM shop | Buy, cancel, insufficient funds, balance deduction and free equip pass; the removed landscape category cannot be reopened |
| Original saves | Every old ball selection tested, retained IDs mapped, removed purchases refunded once, saturated wallet safe; actual ARM save/reload migration passes |
| Packed background | All 110,400 Mountains pixels decode to the expected palette indices; row offsets/runs and RGB565 palette checked |
| Save data | Round-trip and every-byte corruption rejection; invalid ownership/equipment rejected |
| Actual ARM app | N0110, N0115, N0120 Epsilon memory profiles and the smaller Upsilon profile pass |
| Pause / exit | Frozen physics; save reload; held-key handling; clean app RAM and unchanged firmware RAM |
| Filesystem | Other files preserved, including on a completely full storage volume |
| Rendering | Seven balls and Mountains, rotating patterned trails and 32 visible coins; maximum observed draw time 20 ms |
| Release packing | Installed release/debug bytes are identical at the 107,674-byte RAM budget |

Text uses separate native-size 13/22/32-pixel antialiased masks. The old one-pixel text shadow and bitmap enlargement are removed. Menus, help, HUD, purchase dialog and shop were inspected at native resolution. Gameplay input and terrain tests retain the same results as the previous build.

Release: **71,444 bytes**; installed image: **63,857 bytes**; static app RAM (`.data` + `.bss`): **8,173 bytes**; deepest measured stack: **448 bytes**. No heap is used. The icon remains in flash and is not counted as static RAM. The release is **66.3% smaller** than the previous 211,996-byte version.

SHA-256: `06518c09f8fea232bf1a6d3723c26feb38b0f422338a0a7087efbe970d59c5cf`.

The retained 32-colour Mountains background differs from its intermediate 64-colour rendering by an average of **1.635 per RGB channel out of 255**. Quantization affects scenery; physics uses independent world coordinates. Ball artwork is resized and palette-packed. Full p2 trajectory equality is not claimed: the native bounded circle/segment contact solver replaces the browser engine.

Machine-readable results are in [docs/validation.json](docs/validation.json). ARM timing is measured in the emulator; actual LCD/firmware overhead is not a hardware benchmark. N0115 testing uses its memory budget with the emulator's compatible N0110 processor profile. The target is a 60 Hz simulation and 30 Hz display; physical playtesting is the final check for perceived smoothness.

The published artifact is built against the upstream common safety helpers at `979c19e828fee928d2d9b2843f8a2f6f2fea500e`.
