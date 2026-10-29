#!/usr/bin/env python3
"""Adds your own .nwa games to NumPlay, as games of the kit (NumPlay Studio shows them with the others).

Put .nwa files in the folder nwa/ of the repository. On every build, before the build itself, this script
(run by .github/workflows/build.yml):

- makes each .nwa a game of games/games.json (a big one, "xl": the kit already makes those from their own
  .nwa: the code stays as it is, its RAM goes into NumPlay's shared arena, so the games' RAM never adds up),
  with the name and the icon found inside the .nwa, in games/<id>/;
- makes the few changes in NumPlay's build that a game from a ready-made .nwa needs (it says each one).

Nothing here is committed: it is done again, the same way, on each build.
"""
import colorsys
import glob
import json
import os
import re
import shutil
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAX_GAMES = 100  # NP_MAX_GAMES: the launcher's table of games


def p(*a):
    return os.path.join(ROOT, *a)


def say(msg):
    print("addnwa:", msg, flush=True)


def patch(path, old, new, what):
    s = open(path).read()
    if new in s:
        return
    if old not in s:
        sys.exit(f"addnwa: {path} is not what addnwa.py expects ({what}): {old[:60]!r} not found")
    open(path, "w").write(s.replace(old, new, 1))
    say(f"changed {os.path.relpath(path, ROOT)}: {what}")


# ---------------------------------------------------------------------------- changes in NumPlay's build
def patch_build():
    # the launcher's table: room for more games than the kit's own
    path = p("launcher", "src", "np.h")
    s = open(path).read()
    m = re.search(r"#define NP_MAX_GAMES (\d+)", s)
    if m and int(m.group(1)) < MAX_GAMES:
        open(path, "w").write(s.replace(m.group(0), f"#define NP_MAX_GAMES {MAX_GAMES}", 1))
        say(f"launcher/src/np.h: NP_MAX_GAMES {m.group(1)} -> {MAX_GAMES}")
    # a game from a .nwa may carry unwind tables (.eh_frame): a NumPlay module has no use for them
    patch(p("tools", "module.ld"), "*(.comment)", "*(.comment) *(.eh_frame*)", "drops .eh_frame")
    # a game that doesn't fit the calculator's app space on its own can still be picked where there is room
    # (a Nwagra's 6 MB): it only can't be handed out as an app of its own
    kit = p("tools", "kit.py")
    patch(kit, '''        if g["id"] not in apps:
            sys.exit(f"kit: {g['id']} doesn't fit beside the launcher, and --alone hasn't got its own app")''',
          '''        if g["id"] not in apps:
            print(f"kit: {g['id']} doesn't fit beside the launcher, and --alone hasn't got its own app")
            continue''', "a big game without an app of its own is not fatal")
    patch(kit, '''        if os.path.getsize(b) > a.app_space:
            sys.exit(f"kit: {name} doesn't fit the app area even on its own")''',
          '''        if os.path.getsize(b) > a.app_space:
            print(f"kit: {name} doesn't fit the app area even on its own (only with Enlarged Extra Apps)")
            continue''', "a game bigger than the app space is for Nwagra only")
    patch(kit, '''                sys.exit(f"kit: {g['id']} fits neither beside the launcher nor on its own in {sid}")''',
          '''                print(f"kit: {g['id']} fits neither beside the launcher nor on its own in {sid}")''',
          "a game too big for a space is not fatal")
    # the module of a game that comes as a .nwa (games/<id>/prebuilt.nwa): its own .nwa, with stand-ins for the
    # C library's system calls its libc may leave undefined
    mk = p("Makefile")
    patch(mk, '''elif [ -n "$(filter $*,$(XL_GAMES))" ]; then $(MAKE) --no-print-directory -C games/$* build NWLINK="$(NWLINK)" $(if $(filter $*,$(GAMES_REDIRECT)),$(call GAME_REDIRECT_ARG,$*),); \\
''', '''elif [ -n "$(filter $*,$(XL_GAMES))" ]; then if [ -f games/$*/prebuilt.nwa ]; then mkdir -p games/$*/output && $(ARM_OBJCOPY) --redefine-sym main=nwa_main games/$*/prebuilt.nwa games/$*/output/pre.nwa && $(ARM_CC) $(EADK_CFLAGS) -Os -ffreestanding -c games/$*/stub.c -o games/$*/output/stub.o && $(ARM_LD) -r -z noexecstack games/$*/output/pre.nwa games/$*/output/stub.o -o games/$*/output/$*.nwa; else $(MAKE) --no-print-directory -C games/$* build NWLINK="$(NWLINK)" $(if $(filter $*,$(GAMES_REDIRECT)),$(call GAME_REDIRECT_ARG,$*),); fi; \\
''', "a module from a ready-made .nwa")
    patch(mk, "# the big games' modules are made from these (the kit)",
          "# games that come as a ready-made .nwa (games/<id>/prebuilt.nwa, see tools/addnwa.py)\n"
          "$(foreach g,$(XL_GAMES),$(if $(wildcard games/$(g)/prebuilt.nwa),$(eval APP_$(g) = games/$(g)/output/$(g).nwa:$(g).nwa)))\n"
          "# the big games' modules are made from these (the kit)", "their own .nwa is their app")


# ---------------------------------------------------------------------------- reading a .nwa
class Nwa:
    def __init__(self, path):
        d = self.d = open(path, "rb").read()
        if d[:4] != b"\x7fELF" or d[4] != 1 or d[5] != 1:
            raise ValueError("isn't an ELF file")
        etype, mach = struct.unpack_from("<HH", d, 16)
        if etype != 1 or mach != 40:
            raise ValueError("isn't an object .nwa (ARM relocatable ELF)")
        shoff, = struct.unpack_from("<I", d, 32)
        es, n, si = struct.unpack_from("<HHH", d, 46)
        raw = [struct.unpack_from("<10I", d, shoff + i * es) for i in range(n)]
        strt = d[raw[si][4]:raw[si][4] + raw[si][5]]
        self.secs = {}
        self.list = []
        for r in raw:
            name = strt[r[0]:strt.index(b"\0", r[0])].decode(errors="replace")
            self.list.append((name, r))
            self.secs.setdefault(name, d[r[4]:r[4] + r[5]] if r[1] != 8 else b"")
        self.main = False
        for name, r in self.list:
            if r[1] == 2:
                strs = d[raw[r[6]][4]:raw[r[6]][4] + raw[r[6]][5]]
                for k in range(r[5] // 16):
                    nm, val, sz, info, oth, shn = struct.unpack_from("<IIIBBH", d, r[4] + 16 * k)
                    if strs[nm:strs.index(b"\0", nm)] == b"main" and shn != 0 and info >> 4 == 1:
                        self.main = True
        self.title = self.secs.get(".rodata.eadk_app_name", b"").split(b"\0")[0].decode(errors="ignore").strip()
        self.icon = self.secs.get(".rodata.eadk_app_icon")


def lz4_block(src, size):
    out = bytearray()
    i = 0
    while i < len(src):
        t = src[i]; i += 1
        lit = t >> 4
        if lit == 15:
            while True:
                b = src[i]; i += 1; lit += b
                if b != 255: break
        out += src[i:i + lit]; i += lit
        if i >= len(src): break
        off = src[i] | src[i + 1] << 8; i += 2
        m = t & 15
        if m == 15:
            while True:
                b = src[i]; i += 1; m += b
                if b != 255: break
        m += 4
        for _ in range(m):
            out.append(out[-off])
    if len(out) != size:
        raise ValueError("icon size")
    return bytes(out)


def icon_image(nwa, title):
    from PIL import Image, ImageDraw
    try:
        raw = lz4_block(nwa.icon, 55 * 56 * 2)
        img = Image.new("RGB", (55, 56))
        px = img.load()
        for y in range(56):
            for x in range(55):
                v = raw[2 * (y * 55 + x)] | raw[2 * (y * 55 + x) + 1] << 8
                px[x, y] = ((v >> 11) * 255 // 31, (v >> 5 & 63) * 255 // 63, (v & 31) * 255 // 31)
        return img
    except Exception:  # (no icon, or not the usual one: a tile with the initials)
        h = sum(map(ord, title)) % 360
        r, g, b = (int(c * 255) for c in colorsys.hls_to_rgb(h / 360, 0.45, 0.6))
        img = Image.new("RGB", (55, 56), (r, g, b))
        ImageDraw.Draw(img).text((8, 22), "".join(w[0] for w in title.split())[:3].upper(), fill=(255, 255, 255))
        return img


def make_card(icon, title, colors, size):
    """The game's card, at the size of the kit's own: its colours, its icon, its name."""
    from PIL import Image, ImageDraw, ImageFont
    w, h = size
    top, bottom, accent = (tuple(int(c[i:i + 2], 16) for i in (2, 4, 6)) for c in colors)
    img = Image.new("RGB", (w, h))
    dr = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        dr.line([(0, y), (w, y)], fill=tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)))
    s = int(h * 0.56)
    ic = icon.resize((round(s * 55 / 56), s), Image.NEAREST)
    img.paste(ic, ((w - ic.width) // 2, int(h * 0.08)))
    fs = max(8, int(h * 0.13))
    while True:
        try:
            font = ImageFont.load_default(size=fs)
        except TypeError:  # (an old Pillow: its one small font)
            font = ImageFont.load_default()
            break
        box = dr.textbbox((0, 0), title, font=font)
        if box[2] - box[0] <= w - 8 or fs <= 6:
            break
        fs -= 1
    box = dr.textbbox((0, 0), title, font=font)
    dr.text(((w - (box[2] - box[0])) // 2 - box[0], int(h * 0.88) - (box[1] + box[3]) // 2), title, font=font, fill=accent)
    return img.quantize(colors=colors_n(), method=Image.MEDIANCUT).convert("RGB")


def colors_n():
    return 16


def theme(img):
    px = [c for c in img.convert('RGB').tobytes() and list(zip(*[iter(img.convert('RGB').tobytes())] * 3)) if not (c[0] > 235 and c[1] > 235 and c[2] > 235)] or [(60, 90, 180)]
    r, g, b = (sum(c[i] for c in px) / len(px) / 255 for i in range(3))
    h, l, s = colorsys.rgb_to_hls(r, g, b)
    hx = lambda hh, ll, ss: "0x%02X%02X%02X" % tuple(int(c * 255) for c in colorsys.hls_to_rgb(hh, ll, ss))
    return hx(h, 0.42, max(0.45, s)), hx(h, 0.12, max(0.4, s * 0.8)), hx(h, 0.72, max(0.5, s))


STUB = '''/* Stand-ins for the C library's system calls a game's own libc may leave undefined (weak: the game's own win),
   and a main for the game's own. */
#define W __attribute__((weak))
W int _close(int fd) { (void)fd; return -1; }
W int _fstat(int fd, void *st) { (void)fd; (void)st; return -1; }
W int _isatty(int fd) { (void)fd; return 0; }
W long _lseek(int fd, long off, int whence) { (void)fd; (void)off; (void)whence; return -1; }
W int _read(int fd, void *buf, int n) { (void)fd; (void)buf; (void)n; return -1; }
W int _write(int fd, const void *buf, int n) { (void)fd; (void)buf; return n; }
W int _getpid(void) { return 1; }
W int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }

/* the game's own main (renamed nwa_main) is called with no arguments, as NumPlay's launcher does for its games */
extern int nwa_main(int, char **);
int main(void) { return nwa_main(0, (char **)0); }
'''


def main():
    games_json = p("games", "games.json")
    games = json.load(open(games_json))
    # (an earlier try, a menu inside NumPlay; and what an earlier run of this script made)
    games = [g for g in games if g["id"] != "mesjeux" and not g.get("prebuilt")]
    have = {g["id"] for g in games}
    titles = {g["title"] for g in games}
    files = sorted(glob.glob(p("nwa", "*.nwa")), key=lambda f: os.path.basename(f).lower())
    cards = [g for g in games if "docs/card.png" in g.get("shots", []) and g.get("colors") and
             os.path.exists(p("games", g["id"], "docs", "card.png"))]
    ref = next((g for g in cards if g["id"] == "sudoku"), cards[0] if cards else None)
    if files and not ref:
        sys.exit("addnwa: no game with a docs/card.png to take the launcher's picture from")
    if files:
        patch_build()
    added = []
    for f in files:
        base = os.path.basename(f)
        stem = re.sub(r"\.nwa$", "", base, flags=re.I)
        try:
            nwa = Nwa(f)
            if not nwa.main:
                raise ValueError("has no main")
        except Exception as e:
            say(f"{base}: skipped ({e})")
            continue
        gid = re.sub(r"[^a-z0-9]", "", stem.lower()) or "game"
        if gid[0].isdigit():
            gid = "nwa" + gid
        if gid in have:
            gid = "nwa" + gid
        title = nwa.title or re.sub(r"[-_]+", " ", stem).strip()
        if title in titles:  # (the same name as a game of the kit: the version in the file name tells them apart)
            ver = re.search(r"\d+(?:[_.-]\d+)+", stem)
            title = f"{title} {re.sub(r'[_-]', '.', ver.group(0))}" if ver else f"{title} ({stem})"
        if len(games) >= MAX_GAMES:
            say(f"{base}: skipped (NumPlay holds {MAX_GAMES} games)")
            continue
        img = icon_image(nwa, title)
        top, bottom, accent = theme(img)
        d = p("games", gid)
        for sub in ("src", "docs"):
            os.makedirs(os.path.join(d, sub), exist_ok=True)
        shutil.copy(f, os.path.join(d, "prebuilt.nwa"))
        open(os.path.join(d, "stub.c"), "w").write(STUB)
        img.save(os.path.join(d, "src", "icon.png"))
        from PIL import Image
        size = Image.open(p("games", ref["id"], "docs", "card.png")).size
        make_card(img, title, (top, bottom, accent), size).save(os.path.join(d, "docs", "card.png"))
        entry = {"id": gid, "rank": 100 + len(added), "title": title, "tagline": "Added from a .nwa", "credit": None,
                 "top": top, "bottom": bottom, "accent": accent, "colors": colors_n(), "records": [],
                 "shots": ["docs/card.png"], "repo": "https://github.com/" + os.environ.get("GITHUB_REPOSITORY", "") + "/tree/main/nwa",
                 "default": False, "xl": True, "prebuilt": True}
        games.append(entry)
        have.add(gid)
        titles.add(title)
        added.append(gid)
        say(f"{base}: {title} ({gid})")
    json.dump(games, open(games_json, "w"), indent=2)
    open(games_json, "a").write("\n")
    say(f"{len(added)} game(s) from nwa/, {len(games)} in all")


if __name__ == "__main__":
    main()
