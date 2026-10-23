# Asset provenance

Card faces, card back and green table originate from **AlexDer**,
[UNO Card Game Asset Pack](https://alexder.itch.io/uno-card-game-asset-pack).
The user supplied `Uno Game Assets (1).zip`; its SHA-256 is:

```
873e5243b3bc15140313becdbfb33c2afc7ed858e49c6f67a6ef5f9706ad9837
```

The author's public replies on that page permit use in projects without limits,
including an MIT-licensed open-source game, and appreciate attribution. The pack
does not contain a separate license text. Its font-license comments distinguish
the original Comic Kings font from the assets: this project includes no font
file from the pack and draws its interface with NumPlay's existing native fonts.
Do not describe the pack as authored by this project or as MIT-licensed code.

`tools/assets.py` reads only named PNG entries from the supplied ZIP and generates:

- All 52 colored faces, Wild, Wild +4 and the card back at 44 × 64 pixels,
  with a shared 31-color RGB565 palette and transparent row-RLE encoding.
- A 32 × 24 RGB565 sample of `Table_2.png`, smoothly scaled when drawn.
- A 55 × 56 app icon composed from the supplied card art.

No PDFs, Photoshop files, original-resolution textures, audio or additional
typefaces are included in the game. The pack's “ASSET PACK” banner is not used as
a game logo. The table and cards are the actual supplied artwork, adapted to the
calculator's display and color format.

To reproduce the generated files with Pillow:

```sh
python3 tools/assets.py '/path/to/Uno Game Assets (1).zip'
```

The generated size/provenance report is `tools/assets.json`. UNO belongs to
Mattel; this is an unofficial fan adaptation without Mattel affiliation.
