#!/usr/bin/env python3
"""Brings the Funkin' Crew's files the game is made from (tools/ref.txt: each file's git blob hash and path) from
their funkin.assets repository, at the commit below, into DIR: a sparse, blobless git checkout of just those files
(no audio, nothing else). A DIR that already holds them (a copy of funkin.assets) is only checked. Nothing in it is
run.

  python3 -I tools/fetch.py DIR"""
import hashlib
import subprocess
import sys
from pathlib import Path

REPO = "https://github.com/FunkinCrew/funkin.assets.git"
COMMIT = "724eba9de9e76761d9cefd52169926bc51c760a5"   # the one FunkinCrew/Funkin d05659bb uses
LIST = Path(__file__).resolve().parent / "ref.txt"


def blob(path):
    data = path.read_bytes()
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def wrong(root, files):
    return [p for sha, p in files if not (root / p).is_file() or blob(root / p) != sha]


def git(root, *args):
    subprocess.run(["git", "-C", str(root), *args], check=True, stdout=subprocess.DEVNULL)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    root = Path(sys.argv[1])
    files = [line.split(" ", 1) for line in LIST.read_text().splitlines() if line.strip()]
    bad = wrong(root, files)
    if not bad:
        return
    if root.exists() and any(root.iterdir()) and not (root / ".git").exists():
        sys.exit(f"fetch.py: {root} isn't funkin.assets at {COMMIT[:8]}: {len(bad)} files differ, first {bad[0]}")
    print(f"fetch.py: {len(bad)} of the game's {len(files)} files, from {REPO} at {COMMIT[:8]}", file=sys.stderr)
    root.mkdir(parents=True, exist_ok=True)
    if not (root / ".git").exists():
        git(root, "init", "-q")
        git(root, "remote", "add", "origin", REPO)
    git(root, "sparse-checkout", "init", "--no-cone")
    (root / ".git" / "info" / "sparse-checkout").write_text("".join(f"/{p}\n" for _, p in files))
    git(root, "fetch", "-q", "--depth", "1", "--filter=blob:none", "origin", COMMIT)
    git(root, "-c", "advice.detachedHead=false", "checkout", "-q", "--force", COMMIT)
    bad = wrong(root, files)
    if bad:
        sys.exit(f"fetch.py: {len(bad)} files don't match tools/ref.txt, first {bad[0]}")


if __name__ == "__main__":
    main()
