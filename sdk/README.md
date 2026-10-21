# NumPlay's game kit

`template/` is **Star Catcher**, a whole small NumWorks game in one file (`template/src/main.c`): it draws with NumPlay's 2D strips and fonts, reads the keys, keeps the same speed on every calculator, pauses with Back, quits with Home and saves the best score.

Start a game from it with `python3 tools/new_game.py "My Game"`, which copies it to `games/mygame/`. Or build it as it is: `make -C sdk/template` makes `output/starcatcher.nwa`.

The whole guide: [Make your own NumWorks game](../docs/make-a-game.md).
