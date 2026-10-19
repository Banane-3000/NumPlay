#!/usr/bin/env python3
"""Writes levels/microban.txt: David W. Skinner's Microban (155 levels), in the usual Sokoban text
format, from the copy on Scott Lindhurst's Sokoban levels page. Run once; the file is kept in the
repository, so building needs no network.

  #  wall      @  the keeper     $  a box     .  a goal
     floor     +  keeper on goal *  box on goal

Usage: fetch.py [levels/microban.txt]
"""
import html
import os
import re
import sys
import urllib.request

URL = "http://www.sneezingtiger.com/sokoban/levels/microbanText.html"

page = urllib.request.urlopen(URL, timeout=30).read().decode("latin-1")
pre = page[page.index('<pre class="sokFont">') + len('<pre class="sokFont">'):]
pre = html.unescape(pre[:pre.index("</pre>")])
out = ["; Microban, by David W. Skinner (2000): 155 levels, from his Sokoban page",
       "; (users.bentonrea.com/~sasquatch/sokoban), here as kept at " + URL, ""]
n = 0
for block in re.split(r"\n\s*\n", pre.strip()):
    lines = block.split("\n")
    assert lines[0].startswith("Level ")
    n += 1
    out.append(f"; {n}" + "".join(f" {l.strip()}" for l in lines[1:] if l.startswith("'")))
    out += [l.rstrip() for l in lines[1:] if l.strip() and not l.startswith("'")]
    out.append("")
assert n == 155
path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "levels", "microban.txt")
os.makedirs(os.path.dirname(path), exist_ok=True)
open(path, "w").write("\n".join(out))
print(f"{path}: {n} levels")
