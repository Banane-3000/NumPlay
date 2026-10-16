#!/usr/bin/env python3
"""Register Dino Run in NumPlay, retaining all original playable games.

Run from the NumPlay repo root: python3 tools/integrate_dinorun.py
Safe to run twice. Only edits games/games.json.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / 'games' / 'games.json'
assert (ROOT / 'games' / 'dinorun' / 'src' / 'main.c').exists(), 'Place the Dino files in games/dinorun first'
games = json.loads(PATH.read_text(encoding='utf-8'))
ids = [g['id'] for g in games]
assert len(ids) == len(set(ids)), 'Duplicate game IDs in games.json'
assert 'tetris' in ids, 'Unexpected NumPlay game manifest'

# This preserves every old playable game. Screenshots for the five largest
# launcher cards use a smaller color palette to make flash room for Dino.
# They still display, but with less color fidelity. Individual games unchanged.
compact_previews = {
    'numblocks': 4,
    'balatro': 4,
    'buckshot': 4,
    'crossyroad': 4,
    'numdash': 4,
}
for game in games:
    if game['id'] in compact_previews:
        game['colors'] = compact_previews[game['id']]

new = {
    'id': 'dinorun',
    'title': 'Chrome Dino',
    'tagline': 'Jump, duck, survive',
    'credit': 'Sprite art: Diego KHELLADI (permission pending)',
    'top': '0xE5E5E5',
    'bottom': '0x555555',
    'accent': '0x71A050',
    'records': ['dinorun.sav'],
    'shots': ['docs/shot.png'],
    'app': 'ChromeDino.nwa',
    'repo': 'https://github.com/LilyTheCuteCatgirl/lilys-small-numworks-games/blob/main/games/chrome_dino.py',
}
if 'dinorun' in ids:
    games[ids.index('dinorun')] = new
else:
    games.insert(ids.index('tetris') + 1, new)
assert len(games) <= 32
PATH.write_text(json.dumps(games, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(f'NumPlay: {len(games)} games, Dino integrated; all original games retained.')
print('Flash optimization: five launcher previews reduced to 4 colors.')
print('Commit games/games.json along with the Dino game before running the full workflow.')
