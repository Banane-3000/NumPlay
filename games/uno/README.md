# UNO

A native, English-language UNO game for NumPlay and NumWorks. One human plays
against one, two or three bots on a 320 × 240 card table, using AlexDer's card art.
No browser, sound files, full-screen framebuffer or runtime asset download is needed.

This is an unofficial fan adaptation. Game made by **Bobydeluxe**; card and table
art by **AlexDer**. See [ASSETS.md](ASSETS.md) for asset provenance.

## Play

Install `output/UNO.nwa` with the usual NumWorks external-app installer.
`Continue` restores the saved match; `New game` opens the table settings.
The app asks before replacing an unfinished game. Changing settings does not
alter the rules of an existing match.

Creating a table has three visible steps: **Players**, **Rules**, then **Ready**.
The first step previews your seat and the bot seats while you choose the player
count and bot level. The second keeps all five variants on screen
with On/Off switches and an explanation for the selected rule. The last step
summarizes the whole table before `Deal cards`. Back returns to the previous
step without discarding your choices.

During play, bots sit around the piles, with the current player outlined in
gold. A turn banner stays at the top. `DRAW` and `DISCARD` label the separate
piles, and a named color tile shows the active color, including after Wilds.
Your hand stays in its own lower area; the selected card is enlarged and drawn
in front of its neighbors. Its name, index and playability stay visible below.
You can browse your hand while the bots take their turns. Action buttons show
their shortcuts and keyboard focus skips unavailable actions.

| Key | Action |
| --- | --- |
| Left / Right | Select a card; hold to scroll large hands |
| OK / EXE | Play the selected card or confirm a menu item |
| Up / Down | Enter the action buttons / return to your hand |
| 1 | Draw a card, or accept a pending draw penalty |
| 2 | Keep the card you just drew, if permitted |
| 0 | Call UNO, prepare the call before playing from two cards, or catch a bot |
| Back | Pause; return from menus |
| Home / Power | Save and exit safely |

Gold dots mark playable cards. The lifted card has a readable native-text name.
The hand scrolls to show every card; it is never truncated at a small hand limit.
The color tile is the active color, including after a Wild. `S`, `R`, `+2`, `W`
and `+4` in the card corners identify the special cards.

## Classic rules

This version plays **one game, won by the first empty hand**, using the
classic 108-card deck and Mattel's
[classic instructions](https://service.mattel.com/instruction_sheets/B0001-Eng.pdf)
and [two-player instructions](https://service.mattel.com/instruction_sheets/53641.pdf)
for card effects. There is no point system or multi-round target. It excludes
the extra customizable cards in some newer physical editions.

- Deal seven cards each. A high-card draw selects the initial dealer; subsequent
  rounds move the dealer clockwise. Opening action cards are applied, and an
  opening +4 is returned to the shuffled draw pile.
- Match color, number or symbol. You may draw instead of playing. After drawing,
  you may only play the newly drawn card, or keep it and end the turn.
- Skip skips the next player. Reverse changes direction; with two players it
  acts as a Skip. +2 makes the next player draw two and miss their turn.
- Wild chooses the color. +4 is legal only if its player holds no card of the
  active color. Bluffing is possible: only the target may challenge. A guilty
  offender draws four and the target keeps their turn; otherwise the challenger
  draws six and loses their turn. A human challenge reveals the offender's hand.
- Call UNO when playing down to one card. You can prepare the call with two
  cards or call immediately afterward. A missed call is only penalized when
  caught before the next player draws or plays; the penalty is two cards. Bots
  have a reaction delay, and sometimes miss calls or fail to catch them.
- The first player with no cards wins the game. A final +2/+4 and any challenge
  are resolved before showing the winner. `Play again` starts a fresh game with
  the same table rules, or you can return to the menu.
- Exhausted draw piles recycle all discards except the top card. In the rare
  finite-deck situation where everyone is blocked with nothing left to draw,
  redeal the cards.

## Optional house rules

Settings clearly label altered tables as **Custom**. Each option defaults to off.

| Option | Behavior |
| --- | --- |
| Stack +2 | Play one +2 on a pending +2; the accumulated penalty passes on |
| Stack +4 | Play one +4 on a pending +4; the last chosen color applies |
| Draw until playable | Continue drawing until a playable card appears or the pile cannot refill |
| Force drawn play | A playable drawn card must be played; a drawn Wild still requires a color choice |
| Automatic UNO | Call UNO automatically for every player |

Stacking never mixes +2 and +4, and stops after a last card: its penalty is paid
before the win is confirmed. **Stacked +4 disables challenges**, a stated house-rule choice
that avoids combining incompatible penalty/challenge interpretations.
`Classic` resets all variants; player count and bot difficulty remain independent
choices. Every table uses the same first-empty-hand winning condition.

Casual bots choose less carefully. Normal bots favor their remaining colors and
prioritize action cards. Clever bots also consider public opponent card counts and
the direction of play when choosing action cards. Bots never inspect opponents'
hidden hands to decide a play or challenge.

## Build and validate

Requires ARM GCC, nwlink 1.0.0, Python and pyelftools. Pillow is only needed to
regenerate assets from the user's original ZIP. Generated assets are committed;
the ZIP is not needed for a normal build.

```sh
python3 tools/build.py --toolchain /path/to/arm/bin --nwlink /path/to/nwlink
python3 tools/test.py --cc cc
python3 tests/test_arm.py --nwlink /path/to/nwlink
python3 tests/test_integration.py --nwlink /path/to/nwlink
```

`make build`, `make test` and `make test-arm` wrap these commands. `HOST_CC`,
`PYTHON`, `NWLINK` and `TOOLCHAIN` can be overridden.
The build creates `output/UNO.nwa`, a symbol-bearing debug NWA, installed binaries
for four firmware/RAM profiles, and `output/module.o` for NumPlay launcher
integration. NumPlay includes UNO in its catalogue and exports the standalone
app as `UNO.nwa`; the module uses the shared graphics, storage and backup.

The engine uses a bounded 108-card deck and fixed-size hands, with no heap.
Card sprites share a 31-color palette and use transparent row RLE; only a single
44-byte row is decoded at a time. A small original table sample is smoothly
scaled, and the native fonts draw the UI directly. LCD drawing uses 16-row
strips, and an unchanged table sleeps instead of redrawing continuously.

Saves use a versioned 768-byte record with explicit little-endian fields, CRC32
and full deck/state validation before loading. Full-storage failures offer retry
or an explicit exit without saving. Standalone builds use NumPlay's existing
Epsilon/Upsilon safe-exit and storage helpers.
Save version 2 also reads version 1: ongoing hands are retained, old point totals
and targets are dropped, and a previously won round becomes a finished game.

See [VALIDATION.md](VALIDATION.md) for measured size and validation limits.
