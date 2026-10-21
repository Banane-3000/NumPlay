#!/usr/bin/env python3
"""Draws each vehicle put together from tools/art.py, big, with its axles marked: a check of the numbers.

    python3 -I tools/preview.py REF OUT.png"""
import math
import os
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import art  # noqa: E402

PPM = 70   # the preview's pixels per meter


def load(ref, name):
    if name == "buggy-tire":
        return Image.open(os.path.join(ref, "wiki", "Dune_Buggy_(Sprite).png")).convert("RGBA").crop((6, 133, 86, 213))
    return Image.open(os.path.join(ref, "wiki", name)).convert("RGBA")


def paste(canvas, im, scale, anchor_src, anchor_dst, angle=0):
    """im scaled, turned by angle degrees (counterclockwise) around anchor_src, which lands on anchor_dst."""
    w, h = max(1, round(im.width * scale)), max(1, round(im.height * scale))
    s = im.resize((w, h), Image.LANCZOS)
    ax, ay = anchor_src[0] * scale, anchor_src[1] * scale
    if angle:
        big = Image.new("RGBA", (w * 3, h * 3))
        big.alpha_composite(s, (w, h))
        big = big.rotate(angle, resample=Image.BICUBIC, center=(w + ax, h + ay))
        s, ax, ay = big, w + ax, h + ay
    canvas.alpha_composite(s, (round(anchor_dst[0] - ax), round(anchor_dst[1] - ay)))


def bill(canvas, ref, v, at_px, k):
    """Bill seated, his hips at at_px (canvas pixels), k canvas pixels a meter."""
    d = v["driver"]
    B = art.BILL
    body = load(ref, B["body"])
    head = load(ref, B["head"])
    bs = B["body_m"] * d["size"] * k / body.height
    paste(canvas, body, bs, B["body_hips"], at_px, d["lean"])
    a = math.radians(d["lean"])
    nx = (B["body_neck"][0] - B["body_hips"][0]) * bs
    ny = (B["body_neck"][1] - B["body_hips"][1]) * bs
    neck = (at_px[0] + nx * math.cos(a) + ny * math.sin(a), at_px[1] - nx * math.sin(a) + ny * math.cos(a))
    hs = B["head_m"] * d["size"] * k / head.height
    paste(canvas, head, hs, B["head_neck"], neck)


def main():
    ref, out = sys.argv[1], sys.argv[2]
    tiles = []
    for v in art.VEHICLES:
        ch = load(ref, v["chassis"])
        ax = v["axles"]
        cppm = (ax[-1][0] - ax[0][0]) / v["wheelbase"]   # the chassis picture's pixels a meter
        k = PPM / cppm                                     # chassis pixels -> preview pixels
        W, H = int(ch.width * k) + 120, int(ch.height * k) + 160
        c = Image.new("RGBA", (W, H), (150, 200, 235, 255))
        ox, oy = 60, 110
        to = lambda p: (ox + p[0] * k, oy + p[1] * k)  # noqa: E731
        if v["id"] == "tank":
            paste(c, load(ref, "Car-tank-bg.png"), k, (0, 0), to((0, 0)))
        bill(c, ref, v, to(v["driver"]["hips"]), PPM)
        paste(c, ch, k, (0, 0), to((0, 0)))
        wheel = load(ref, v["wheel"])
        ws = v["wheel_m"] * PPM / v["wheel_px"]
        for p in ax:
            paste(c, wheel, ws, (wheel.width / 2, wheel.height / 2), to(p))
        dr = ImageDraw.Draw(c)
        for p in ax:
            x, y = to(p)
            r = v["wheel_m"] * PPM
            dr.ellipse((x - r, y - r, x + r, y + r), outline=(255, 0, 255, 255))
            dr.line((x - 4, y, x + 4, y), fill=(255, 0, 255, 255))
        dr.text((4, 4), v["name"], fill=(0, 0, 0, 255))
        tiles.append(c)
    W = max(t.width for t in tiles) * 2
    rows = [tiles[i:i + 2] for i in range(0, len(tiles), 2)]
    H = sum(max(t.height for t in r) for r in rows)
    sheet = Image.new("RGBA", (W, H), (255, 255, 255, 255))
    y = 0
    for r in rows:
        x = 0
        for t in r:
            sheet.alpha_composite(t, (x, y))
            x += W // 2
        y += max(t.height for t in r)
    sheet.convert("RGB").save(out)


if __name__ == "__main__":
    main()
