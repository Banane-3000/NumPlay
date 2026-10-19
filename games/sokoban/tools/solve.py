#!/usr/bin/env python3
"""Solves every level as the game gets it (tools/levels.py) and writes tests/solutions.txt: a line a
level, its moves in the usual letters (u r d l to walk, U R D L to push), for the host test (make test),
which plays each one through the game's own rules.

The search goes a push at a time, the most promising first: how many pushes the boxes need at least,
each to a goal of its own (an assignment of the boxes to the goals), plus a little for the pushes made.
Squares a box could never get from to a goal, and boxes frozen together (four cells in a square, all
boxes or walls, one box off its goal), are left out. The solutions aren't the shortest; they only show
every level can be solved as packed.

Usage: solve.py [tests/solutions.txt]
"""
import heapq
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import levels  # noqa: E402

WAYS = ((-1, 0, "u"), (0, 1, "r"), (1, 0, "d"), (0, -1, "l"))
BIG = 1 << 20


def assignment(cost):
    """The cheapest way to give each row a column of its own (the Hungarian method)."""
    n = len(cost)
    u, v, p, way = [0] * (n + 1), [0] * (n + 1), [0] * (n + 1), [0] * (n + 1)
    for i in range(1, n + 1):
        p[0], j0 = i, 0
        minv, used = [BIG * BIG] * (n + 1), [False] * (n + 1)
        while True:
            used[j0] = True
            i0, delta, j1 = p[j0], BIG * BIG, 0
            for j in range(1, n + 1):
                if not used[j]:
                    c = cost[i0 - 1][j - 1] - u[i0] - v[j]
                    if c < minv[j]:
                        minv[j], way[j] = c, j0
                    if minv[j] < delta:
                        delta, j1 = minv[j], j
            for j in range(n + 1):
                if used[j]:
                    u[p[j]] += delta
                    v[j] -= delta
                else:
                    minv[j] -= delta
            j0 = j1
            if not p[j0]:
                break
        while j0:
            j1 = way[j0]
            p[j0] = p[j1]
            j0 = j1
    return -v[0]


def solve(rows, limit=3000000):
    cells = {(y, x) for y, r in enumerate(rows) for x, c in enumerate(r) if c != " "}
    at = {(y, x): c for y, r in enumerate(rows) for x, c in enumerate(r)}
    goals = sorted(p for p in cells if at[p] in ".*+")
    goal_set = frozenset(goals)
    boxes = frozenset(p for p in cells if at[p] in "$*")
    keeper = next(p for p in cells if at[p] in "@+")

    # the pushes from each square to each goal, the box pulled back from the goal
    dist = {}
    for gi, g in enumerate(goals):
        d, todo = {g: 0}, [g]
        while todo:
            nxt = []
            for p in todo:
                for dy, dx, _ in WAYS:
                    q, k = (p[0] + dy, p[1] + dx), (p[0] + 2 * dy, p[1] + 2 * dx)
                    if q in cells and k in cells and q not in d:
                        d[q] = d[p] + 1
                        nxt.append(q)
            todo = nxt
        for p, n in d.items():
            dist.setdefault(p, [BIG] * len(goals))[gi] = n

    def area(k, bx):
        seen, todo = {k}, [k]
        while todo:
            y, x = todo.pop()
            for dy, dx, _ in WAYS:
                n = (y + dy, x + dx)
                if n in cells and n not in bx and n not in seen:
                    seen.add(n)
                    todo.append(n)
        return seen

    def frozen(b, bx):
        """A square of four cells around b, all walls or boxes, with a box off its goal."""
        for oy in (-1, 0):
            for ox in (-1, 0):
                sq = [(b[0] + oy + a, b[1] + ox + c) for a in (0, 1) for c in (0, 1)]
                if all(q not in cells or q in bx for q in sq) and any(q in bx and q not in goal_set for q in sq):
                    return True
        return False

    def guess(bx):
        return assignment([dist[b] for b in bx])

    first = area(keeper, boxes)
    start = (boxes, min(first))
    parent = {start: None}
    heap = [(guess(boxes), 0, 0, start, first)]
    tie = 0
    while heap and len(parent) < limit:
        _, c, _, key, ar = heapq.heappop(heap)
        bx = key[0]
        if bx <= goal_set:
            return moves(key, parent, keeper, boxes, cells)
        for b in bx:
            for dy, dx, _ in WAYS:
                frm, to = (b[0] - dy, b[1] - dx), (b[0] + dy, b[1] + dx)
                if frm in ar and to in cells and to not in bx and to in dist:
                    nb = (bx - {b}) | {to}
                    if frozen(to, nb):
                        continue
                    na = area(b, nb)
                    nk = (nb, min(na))
                    if nk in parent:
                        continue
                    h = guess(nb)
                    if h >= BIG:
                        continue
                    parent[nk] = (key, b, (dy, dx))
                    tie += 1
                    heapq.heappush(heap, (3 * h + c + 1, c + 1, tie, nk, na))
    return None


def walk(frm, to, bx, cells):
    """The keeper's way from frm to to around the boxes, in letters."""
    prev, todo = {frm: None}, [frm]
    while todo:
        nxt = []
        for p in todo:
            if p == to:
                out = []
                while prev[p]:
                    p, ch = prev[p]
                    out.append(ch)
                return "".join(reversed(out))
            for dy, dx, ch in WAYS:
                n = (p[0] + dy, p[1] + dx)
                if n in cells and n not in bx and n not in prev:
                    prev[n] = (p, ch)
                    nxt.append(n)
        todo = nxt
    raise AssertionError("no way")


def moves(key, parent, keeper, boxes, cells):
    pushes = []
    while parent[key]:
        key, b, d = parent[key]
        pushes.append((b, d))
    out, k, bx = "", keeper, set(boxes)
    for b, (dy, dx) in reversed(pushes):
        out += walk(k, (b[0] - dy, b[1] - dx), bx, cells)
        out += next(ch for y, x, ch in WAYS if (y, x) == (dy, dx)).upper()
        bx.remove(b)
        bx.add((b[0] + dy, b[1] + dx))
        k = b
    return out


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "tests", "solutions.txt")
    only = [int(a) for a in sys.argv[2:]]
    lines = ["# tools/solve.py: each level's moves (u r d l walk, U R D L push), for tests/play.c"]
    for i, rows in enumerate([levels.pack(r) for r in levels.read()], 1):
        if only and i not in only:
            continue
        s = solve(rows)
        assert s, f"level {i} not solved"
        print(f"level {i}: {len(s)} moves, {sum(ch.isupper() for ch in s)} pushes", flush=True)
        lines.append(f"{i}: {s}")
    open(path, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
