"""Reading the Sonic Retro disassembly of Sonic the Hedgehog (REF, see fetch.py): its three
compression formats, and the sprite mappings, animations and tables written as assembly.

- nemesis(): 8x8 patterns (4 bits a pixel), coded with a table of variable-length codes
- kosinski(): LZ77 with bit fields (16-by-16 block layouts of the 256x256 chunks)
- enigma(): words of tile maps (the 16x16 blocks, the special stages, the title screen)
"""
import os
import re


def nemesis(d):
    """The patterns of a Nemesis file, 32 bytes each."""
    head = d[0] << 8 | d[1]
    xor, rows = head & 0x8000, (head & 0x7FFF) * 8
    p = 2
    table = {}
    pal = 0
    b = d[p]
    p += 1
    while b != 0xFF:
        if b & 0x80:
            pal = b & 0x0F
            b = d[p]
            p += 1
            continue
        rep = (b & 0x70) >> 4
        length = b & 0x0F
        code = d[p]
        p += 1
        table[(length, code)] = (pal, rep + 1)
        b = d[p]
        p += 1
    # the bits, most significant first
    bits = []
    for c in d[p:]:
        for i in range(7, -1, -1):
            bits.append(c >> i & 1)
    out = bytearray()
    row, n, prev, q = 0, 0, 0, 0
    while len(out) < rows * 4:
        if q + 6 <= len(bits) and all(bits[q:q + 6]):
            q += 6
            v = 0
            for _ in range(7):
                v = v << 1 | bits[q]
                q += 1
            pal, cnt = v & 0x0F, ((v >> 4) & 7) + 1
        else:
            code, length = 0, 0
            while True:
                code = code << 1 | bits[q]
                q += 1
                length += 1
                if (length, code) in table:
                    pal, cnt = table[(length, code)]
                    break
                if length > 8:
                    raise ValueError("bad Nemesis code")
        for _ in range(cnt):
            row = (row << 4 | pal) & 0xFFFFFFFF
            n += 1
            if n == 8:
                if xor:
                    prev ^= row
                    row = prev
                out += row.to_bytes(4, "big")
                row, n = 0, 0
                if len(out) >= rows * 4:
                    break
    return bytes(out)


def kosinski(d):
    out = bytearray()
    p = 2
    desc, left = d[0] | d[1] << 8, 16

    def bit():
        nonlocal desc, left, p
        b = desc & 1
        desc >>= 1
        left -= 1
        if left == 0:
            desc, left = d[p] | d[p + 1] << 8, 16
            p += 2
        return b

    while True:
        if bit():
            out.append(d[p])
            p += 1
            continue
        if bit():
            lo, hi = d[p], d[p + 1]
            p += 2
            off = (0xE000 | (hi & 0xF8) << 5 | lo) - 0x10000
            if hi & 7:
                cnt = (hi & 7) + 2
            else:
                c = d[p]
                p += 1
                if c == 0:
                    break
                if c == 1:
                    continue
                cnt = c + 1
        else:
            cnt = (bit() << 1 | bit()) + 2
            off = d[p] - 0x100
            p += 1
        for _ in range(cnt):
            out.append(out[off])
    return bytes(out)


def enigma(d, start=0):
    """The words of an Enigma tile map; start is the art tile added to each."""
    nbits, mask = d[0], d[1]
    inc = (d[2] << 8 | d[3]) + start
    lit = (d[4] << 8 | d[5]) + start
    bits = []
    for c in d[6:]:
        for i in range(7, -1, -1):
            bits.append(c >> i & 1)
    q = 0

    def take(n):
        nonlocal q
        v = 0
        for _ in range(n):
            v = v << 1 | bits[q]
            q += 1
        return v

    def inline():
        v = start
        for flag, value in ((0x10, 0x8000), (0x08, 0x4000), (0x04, 0x2000), (0x02, 0x1000), (0x01, 0x0800)):
            if mask & flag and take(1):
                v += value
        return (v + take(nbits)) & 0xFFFF

    out = []
    while True:
        if take(1) == 0:
            kind = take(1)
            cnt = take(4) + 1
            if kind == 0:
                for _ in range(cnt):
                    out.append(inc & 0xFFFF)
                    inc += 1
            else:
                out += [lit & 0xFFFF] * cnt
        else:
            kind = take(2)
            cnt = take(4)
            if kind == 3 and cnt == 0xF:
                break
            cnt += 1
            if kind == 3:
                for _ in range(cnt):
                    out.append(inline())
            else:
                v = inline()
                for _ in range(cnt):
                    out.append(v & 0xFFFF)
                    v += (0, 1, -1)[kind]
    return out


# ---------------------------------------------------------------- assembly sources

def number(s, sym=None):
    """An assembler expression of numbers, symbols, + - * / << >> & | and parentheses."""
    s = s.strip()
    expr = re.sub(r"\$([0-9A-Fa-f]+)", lambda m: str(int(m.group(1), 16)), s)
    expr = re.sub(r"%([01]+)", lambda m: str(int(m.group(1), 2)), expr)
    if sym:
        expr = re.sub(r"[A-Za-z_][A-Za-z_0-9.]*", lambda m: str(sym[m.group(0)]) if m.group(0) in sym else m.group(0), expr)
    expr = re.sub(r"\b0+(\d)", r"\1", expr)
    expr = expr.replace("/", "//")
    return int(eval(expr, {"__builtins__": {}}))


def strip(line):
    """A line without its comment."""
    out, quote = "", False
    for c in line:
        if c == '"':
            quote = not quote
        if c == ";" and not quote:
            break
        out += c
    return out.rstrip()


def constants(ref):
    """Every `name = value` and `name equ value` of the constants files that is a number."""
    sym = {}
    for f in ("_Constants.asm", "_Variables.asm"):
        for line in open(os.path.join(ref, f), encoding="latin1"):
            m = re.match(r"^\s*([A-Za-z_][A-Za-z_0-9]*)\s*(?:=|equ)\s*([^;]+)", strip(line))
            if not m:
                continue
            try:
                sym[m.group(1)] = number(m.group(2), sym)
            except Exception:
                pass
    return sym


def mappings(path):
    """The frames of a mappings file: each a list of (x, y, w, h, tile word) pieces."""
    lines = [strip(l) for l in open(path, encoding="latin1")]
    order, frames, cur = [], {}, None
    for l in lines:
        m = re.match(r"^\s*mappingsTableEntry\.w\s+(\S+)", l)
        if m:
            order.append(m.group(1))
            continue
        m = re.match(r"^([.A-Za-z_0-9]+):?\s+spriteHeader", l)
        if m:
            cur = m.group(1)
            frames[cur] = []
            continue
        m = re.match(r"^\s*spritePiece\s+(.*)$", l)
        if m and cur:
            a = [number(x) for x in m.group(1).split(",")]
            x, y, w, h, tile, xf, yf, pal, pri = a
            word = (pri & 1) << 15 | (pal & 3) << 13 | (yf & 1) << 12 | (xf & 1) << 11 | (tile & 0x7FF)
            frames[cur].append((x, y, w, h, word))
    return [frames[o] for o in order]


def dplcs(path):
    """Sonic's dynamic pattern load cues: for each frame, (first tile, count) runs of his art."""
    lines = [strip(l) for l in open(path, encoding="latin1")]
    order, frames, cur = [], {}, None
    for l in lines:
        m = re.match(r"^\s*mappingsTableEntry\.w\s+(\S+)", l)
        if m:
            order.append(m.group(1))
            continue
        m = re.match(r"^([.A-Za-z_0-9]+):?\s+dplcHeader", l)
        if m:
            cur = m.group(1)
            frames[cur] = []
            continue
        m = re.match(r"^\s*dplcEntry\s+(.*)$", l)
        if m and cur:
            n, t = [number(x) for x in m.group(1).split(",")]
            frames[cur].append((t, n))
    return [frames[o] for o in order]


AF = {"afEnd": 0xFF, "afBack": 0xFE, "afChange": 0xFD, "afRoutine": 0xFC, "afReset": 0xFB, "af2ndRoutine": 0xFA}


def animations(path, sym=None):
    """The scripts of an animation file, as bytes (the game's own format)."""
    lines = [strip(l) for l in open(path, encoding="latin1")]
    order, scripts, cur = [], {}, None
    s = dict(AF)
    if sym:
        s.update(sym)
    for l in lines:
        m = re.match(r"^([A-Za-z_0-9]+):\s*dc\.w\s+(\S+)-", l)
        if m:
            order.append(m.group(2))
            continue
        m = re.match(r"^\s*dc\.w\s+(\S+)-", l)
        if m:
            order.append(m.group(1))
            continue
        m = re.match(r"^([.A-Za-z_0-9]+):\s*dc\.b\s+(.*)$", l)
        if m:
            cur = m.group(1)
            scripts[cur] = [number(x, s) & 0xFF for x in m.group(2).split(",")]
            continue
        m = re.match(r"^\s*dc\.b\s+(.*)$", l)
        if m and cur:
            scripts[cur] += [number(x, s) & 0xFF for x in m.group(1).split(",")]
    return [bytes(scripts[o]) for o in order]
