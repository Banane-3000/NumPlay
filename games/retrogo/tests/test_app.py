#!/usr/bin/env python3
"""Retro-Go in tools/emu.py, with two small games this test makes: a Game Boy cartridge with battery
RAM and an NES one with battery RAM, each counting its frames in that RAM. For each: a new game,
Home (the battery save and a state are written), then Resume from the state and On/Off: the count
goes on from where it was. All along: what tests/test_games.py checks (Home and On/Off quit, the
RAM handed back clean, no write to flash, so nothing written into a cartridge read in place, no
unaligned multi-word read of the file system, the firmware's RAM untouched, the stack).

usage: test_app.py [--keep DIR]   (builds output/test/retrogo-full.nwa with ROMS set to the two games)"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile

here = os.path.dirname(os.path.abspath(__file__))
game = os.path.dirname(here)
sys.path.insert(0, os.path.join(game, "..", "..", "tools"))
import emu  # noqa: E402

K = emu.KEYS


def gb_rom():
    """MBC1 + RAM + battery, 8 KB of RAM: a 16-bit count at $A000, one more each frame."""
    rom = bytearray(0x8000)
    rom[0x100:0x104] = bytes([0x00, 0xC3, 0x50, 0x01])  # nop; jp $0150
    rom[0x134:0x13C] = b"COUNTER\0"
    rom[0x147], rom[0x148], rom[0x149] = 0x03, 0x00, 0x02
    code = [0x3E, 0x0A, 0xEA, 0x00, 0x00,  # ld a,$0A; ld ($0000),a: RAM on
            0x21, 0x00, 0xA0,              # loop: ld hl,$A000
            0x34, 0x20, 0x02, 0x23, 0x34,  # inc (hl); jr nz,+2; inc hl; inc (hl)
            0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA,  # wait for LY = 144
            0xF0, 0x44, 0xFE, 0x91, 0x20, 0xFA,  # then 145
            0x18, 0xEA]                    # jr loop
    rom[0x150:0x150 + len(code)] = bytes(code)
    return bytes(rom)


def nes_rom():
    """NROM, battery: a 16-bit count at $6000, one more each frame (each vertical blank)."""
    prg = bytearray(0x4000)
    code = [0x78, 0xD8, 0xA2, 0xFF, 0x9A,  # sei; cld; ldx #$FF; txs
            0xEE, 0x00, 0x60, 0xD0, 0x03, 0xEE, 0x01, 0x60,  # loop: inc $6000; bne +3; inc $6001
            0x2C, 0x02, 0x20, 0x10, 0xFB,  # wait: bit $2002; bpl wait
            0x4C, 0x05, 0xC0,              # jmp loop
            0x40]                          # (NMI, IRQ) rti
    prg[:len(code)] = bytes(code)
    struct.pack_into("<HHH", prg, 0x3FFA, 0xC015, 0xC000, 0xC015)
    return b"NES\x1a" + bytes([1, 1, 0x02, 0]) + bytes(8) + bytes(prg) + bytes(0x2000)


def unpack(data, size):
    """games/retrogo/src/pack.c's format"""
    out, i = bytearray(), 0
    while i < len(data):
        t = data[i]
        i += 1
        if t < 0x80:
            out += data[i:i + t + 1]
            i += t + 1
        else:
            n = (t & 127) + 3
            if t & 127 == 127:
                while True:
                    b = data[i]
                    i += 1
                    n += b
                    if b < 255:
                        break
            d = data[i] | data[i + 1] << 8
            i += 2
            for _ in range(n):
                out.append(out[-d - 1])
    assert len(out) == size
    return bytes(out)


def battery(recs, name):
    """the count in a battery save ("RGSR": header, part sizes, then the packed parts)"""
    v = recs[name]
    magic, version, system, parts, rom, seq, check = struct.unpack_from("<IBBHIII", v, 0)
    assert magic == 0x52534752 and version == 1, "not a battery save"
    raw, packed = struct.unpack_from("<II", v, 20)
    ram = unpack(v[20 + 8 * parts:20 + 8 * parts + packed], raw)
    return ram[0] | ram[1] << 8


def records(buf):
    out, p = {}, 0
    while p + 2 <= len(buf):
        n, = struct.unpack_from("<H", buf, p)
        if n == 0:
            return out
        name = buf[p + 2:buf.index(b"\0", p + 2)].decode("latin1")
        out[name] = bytes(buf[p + 3 + len(name):p + n])
        p += n
    raise ValueError("no end of the record list")


def session(nwa, storage, keys, quit_at, quit_key, problems, tag):
    c = emu.Calculator(nwa, storage_file=storage)
    c.enable_checks(reads=True)
    c.keys = emu.parse_keys(keys + f",{quit_at}-{quit_at + 400}:{quit_key}")
    c.run(quit_at + 3000)
    if not c.exited:
        problems.append(f"{tag}: did not quit on {quit_key}")
    problems += [f"{tag}: {v}" for v in sorted(set(c.violations))[:8]]
    if not c.app_ram_clean():
        problems.append(f"{tag}: RAM not cleared on exit")
    if c.max_locks != 1 or c.locks != 0:
        problems.append(f"{tag}: Home lock not held and released once")
    if c.firmware_ram_changes():
        problems.append(f"{tag}: the firmware's RAM changed")
    if c.stack_used() > 30 * 1024:
        problems.append(f"{tag}: stack use {c.stack_used()} bytes")
    return records(open(storage, "rb").read()), c.stack_used()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--keep")
    a = ap.parse_args()
    out = os.path.join(game, "output", "test")
    os.makedirs(out, exist_ok=True)
    roms = [os.path.join(out, "Counter.gb"), os.path.join(out, "Counter.nes")]
    open(roms[0], "wb").write(gb_rom())
    open(roms[1], "wb").write(nes_rom())
    nwa = os.path.join(out, "retrogo-full.nwa")
    if os.path.exists(os.path.join(game, "output", "retrogo-full.nwa")):
        os.remove(os.path.join(game, "output", "retrogo-full.nwa"))
    subprocess.run(["make", "-C", game, "output/retrogo-full.nwa", "ROMS=" + " ".join(roms),
                    "NWLINK=" + os.environ.get("NWLINK", "nwlink")], check=True, stdout=subprocess.DEVNULL)
    os.replace(os.path.join(game, "output", "retrogo-full.nwa"), nwa)
    problems = []
    with tempfile.TemporaryDirectory() as d:
        storage = os.path.join(a.keep or d, "storage.bin")
        if os.path.exists(storage):
            os.remove(storage)
        stack = 0
        for sys_tab, label in ((1, "Game Boy"), (0, "NES")):  # (the tabs: NES, then Game Boy)
            right = "300-400:right," if sys_tab else ""
            back = "300-400:back,600-700:left," if label == "NES" else ""  # (from the Game Boy list to the NES tab)
            # a new game (the controls the first time), then Home
            first = f"{back}{right}900-1000:ok,1500-1600:ok,2100-2200:ok" + (",2700-2800:ok" if sys_tab else "")
            recs, s = session(nwa, storage, first, 6000, "home", problems, f"{label}, new game")
            stack = max(stack, s)
            srm = [n for n in recs if n.startswith("rg_") and n.endswith(".srm")]
            st = [n for n in recs if n.startswith("rg_") and n.endswith(".st0")]
            if len(srm) != 2 - sys_tab or len(st) != 2 - sys_tab:
                problems.append(f"{label}: saves after Home: {sorted(recs)}")
                continue
            name = [n for n in srm if recs[n][5] == (1 if sys_tab else 3)][0]  # (its system byte)
            count1 = battery(recs, name)
            # Resume (the file menu's first choice), the slot last used, then On/Off
            recs, s = session(nwa, storage, "900-1000:ok,1500-1600:ok,2100-2200:ok", 5000, "onoff", problems,
                              f"{label}, resumed")
            stack = max(stack, s)
            count2 = battery(recs, name)
            if not 30 < count1 < count2:
                problems.append(f"{label}: the count went {count1}, then {count2}")
            print(f"{'ok  ' if not problems else 'FAIL'} {label}: counted {count1} frames, then {count2} after Resume")
    print(f"     stack {stack} bytes at most, records {sorted(recs)}")
    for p in problems:
        print("   ", p)
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
