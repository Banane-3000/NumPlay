> **Hill Climb Racing is a [NumPlay](../../README.md) game that comes on its own.** It is too big to sit next to NumPlay. Get `HillClimbRacing.nwa` from the [latest release](https://github.com/Mason363/NumPlay/releases/latest).

<p align="center">
  <img src="src/icon.png" width="84" alt="Hill Climb Racing icon">
</p>

<h1 align="center">Hill Climb Racing</h1>

<p align="center">
  <b>Newton Bill's ride up the hills, on the NumWorks calculator.</b><br>
  Ten stages, eight vehicles and every one of their upgrades: drive as far as your fuel and Bill's neck allow.
</p>

<p align="center">
  <a href="https://github.com/Mason363/NumPlay/releases/latest/download/HillClimbRacing.nwa"><b>Download HillClimbRacing.nwa</b></a>
  &nbsp;·&nbsp; <a href="#controls">Controls</a>
</p>

<p align="center">
  <img src="../../docs/media/hillclimb.gif" width="640" alt="The Hill Climber up the Countryside's hills, then the Motocross Bike's back flip on the Moon">
</p>

## Highlights

* **Real physics, like the game's:** the chassis and each wheel are bodies of their own, the wheels hang on springs, and the engine turns them against the chassis, so the nose rises when you floor it and you can flip over backwards on a steep hill. In the air, gas tilts you back and brake tilts you forward.
* **Bill's neck:** land on his head, or let the roof of a cave touch it, and it's *Driver down*. Run out of fuel and you roll to a stop. Fuel cans come at the stage's own spacing, as in the game.
* **Coins of 5, 25, 100 and 500**, bigger the further you go, in rows and stacks along the road. **Air time** pays from a second in the air on (25, 50, 75, 100, then *Big air time* 200 to 500, then *Insane air time* from 1,000 up to 25,000), every **flip** and **back flip** 1,000, and a **neck flip** (going down in the middle of one) 2,500. Each **level** of the road pays its reward when you reach its end.
* **Ten stages**, each with its own ground, sky, grip, gravity and spacing of fuel, with the game's prices:

  | Stage | Price | |
  | --- | --- | --- |
  | Countryside | free | Rolling hills that get steep and jagged further on |
  | Desert | 35,000 | Steep dunes and rope bridges over canyons, sagging under you |
  | Arctic | 75,000 | The Countryside's hills, but on slippery snow |
  | Highway | 125,000 | Flat and fast at first; the fuel cans get further and further apart |
  | Cave | 150,000 | A ceiling over your head: hit it with Bill and it's over. Fuel every 150, 120 and 90 m |
  | Moon | 175,000 | Low gravity: long jumps, big air time and flips, the Earth in the sky |
  | Mars | 250,000 | Lighter than home, with tunnels now and then |
  | Alien Planet | 750,000 | A slimy, sloped world under a green night |
  | Arctic Cave | 800,000 | A cave of ice, slippery, with icicles hanging from its roof |
  | Forest | 850,000 | Bunches of pine trees in the way: ram them or push them over |

* **Eight vehicles** that each drive their own way, with the game's prices and their four upgrades, each in the game's number of levels and at its prices:

  | Vehicle | Price | Upgrades |
  | --- | --- | --- |
  | Hill Climber | free | Engine, Suspension, Tires, 4WD |
  | Motocross Bike | 75,000 | Engine, Suspension, Tires, Air Control |
  | Monster Truck | 100,000 | Engine, Suspension, Tires, 4WD |
  | Quad Bike | 175,000 | Engine, Suspension, Tires, Air Control (four-wheel drive) |
  | Race Car | 250,000 | Engine, Suspension, Tires, Downforce |
  | Rally Car | 750,000 | Engine, Suspension, Tires, Downforce |
  | Dune Buggy | 800,000 | Engine, Suspension, Tires, Fuel |
  | Tank | 1,000,000 | Engine, Suspension, Tracks, Fuel |

* **The game's own look:** its vehicles, wheels and Bill, its coins and fuel cans, each stage's sky, surface and ground, the pedals and the RPM and boost meters on the dashboard, the garage and the stage cards, all from the game itself.
* **Saved as you go:** coins, what you own and its upgrades, and your best distance on each stage with each vehicle. A run left halfway keeps its coins. The save is also copied into `hillclimb_saves.py`, so installing the app again doesn't erase it.

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/countryside.png" width="100%" alt="The Hill Climber on the Countryside, coins ahead"><br>The Countryside</td>
    <td align="center" width="33%"><img src="docs/bridge.png" width="100%" alt="The Hill Climber on a rope bridge over a canyon in the Desert"><br>A rope bridge in the Desert</td>
    <td align="center" width="33%"><img src="docs/moon.png" width="100%" alt="The Motocross Bike's back flip on the Moon"><br>A back flip on the Moon</td>
  </tr>
  <tr>
    <td align="center"><img src="docs/cave.png" width="100%" alt="The Hill Climber in the Cave, under its rocky ceiling"><br>The Cave</td>
    <td align="center"><img src="docs/forest.png" width="100%" alt="The Hill Climber ramming the Forest's pine trees"><br>The Forest's trees</td>
    <td align="center"><img src="docs/garage.png" width="100%" alt="The garage: the Hill Climber and its four upgrades"><br>The garage</td>
  </tr>
</table>

## Controls

| Key | Action |
| --- | --- |
| **Right**, **OK** or **EXE** (or 6) | Gas; in the air, tilt back |
| **Left** (or 4) | Brake, then reverse; in the air, tilt forward |
| **Back** | Pause: Resume, Restart, Garage, Quit game (it asks first) |
| **Home** or **On/Off** | Keep the run's coins and quit |
| In the menus | **Left**, **Right** to choose a vehicle or a stage, **Down** to the upgrades, **OK** to buy or go, **Back** to go back |

## Credits

**Hill Climb Racing** is [Fingersoft](https://fingersoft.com)'s game (2012), with Newton Bill at the wheel. This version is written in C for NumPlay: its physics, stages and menus are made for the calculator, and its pictures (the vehicles, Bill, the coins, the stages' grounds and skies, the pedals, meters, icons and the logo) are the game's own, as the [Hill Climb Racing Wiki](https://hillclimbracing.fandom.com) keeps them; the prices, upgrades, stages' fuel and levels, and the coins' and bonuses' values are the game's as the wiki records them. Its text is set in **Teko** (SIL Open Font License, [license](../../LICENSES/OFL-1.1-Teko.txt)). It is an unofficial fan game, not affiliated with Fingersoft.

`tools/fetch.py` gathers the wiki's pictures and the font, and `tools/pack.py` turns them into `src/data.bin` (committed, so building needs no network).

Build: `make` (needs `arm-none-eabi-gcc` and Node.js for nwlink). `make test` plays it on a computer with the sanitizers (tests/check.py).
