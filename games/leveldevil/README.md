# Level Devil

Run to the door. The level has other plans. Unept's troll platformer on the NumWorks calculator: its flat colours, its tiny black runner, and rooms that change the moment you trust them.

<img src="docs/shot.png" width="320" alt="The runner jumps over spikes that just came up in front of the door">

- **16 levels of 5 rooms**, each level in its own colours, picked on a map of doors. Clear a level to open the next one; the last door has horns.
- **Traps that wait for you:** the floor drops away, spikes pop up in front of the door, walls slide out, the ceiling comes down, the door runs off or turns out to be fake, the keys turn round, gravity flips, saws give chase...
- **Rooms that change after you die in them**, like the original's: the trap you just learned isn't always the one waiting next time.
- **A theme a level:** falling floors, spikes, walls, the door, backwards, fake ground, springs, warps, crushers, saws, chases, jumps in the air, gravity, ice, the edges of the room, and the Devil's own level, which mixes them.
- **Tight controls:** hold jump to jump higher, a moment's grace after running off an edge, and back on your feet the moment you die.
- **Deaths counted,** for the level and in all, with your best (the fewest deaths) for each level. The game remembers the room you reached, so you can stop any time.

## Controls

| Key | Action |
| --- | --- |
| Left, Right (or 4, 6) | Run |
| Up, OK or EXE (or 8, 5) | Jump; hold it to jump higher |
| Arrows, OK | Pick a door on the map |
| Back | Pause: Resume, Restart the room, Levels, How to play, Quit game |
| Home | Quit |

## Credits

[Level Devil - NOT A Troll Game](https://store.steampowered.com/app/3242750/) is **Unept**'s game (2023). This version is written in C for NumPlay, with no code or art from the original: its look and its traps follow the game, and its rooms are made for the calculator's screen in the spirit of the original's.

The rooms are written in [tools/rooms.py](tools/rooms.py), which packs them into a little over 1 KB. `make test` plays every room through on a computer, as it starts and after a death or more, with the keys in [tests/solutions.txt](tests/solutions.txt), so each one can be cleared.

Inside NumPlay it draws with the launcher's shapes and fonts, saves through its file functions and unpacks its rooms with its inflater, so the game itself is about 10 KB.

It comes inside [NumPlay](https://github.com/Mason363/NumPlay), or on its own as `LevelDevil.nwa` from the [latest release](https://github.com/Mason363/NumPlay/releases/latest).

Build: `make` (needs `arm-none-eabi-gcc`, Python 3 and Node.js for nwlink).
