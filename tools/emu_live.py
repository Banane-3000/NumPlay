#!/usr/bin/env python3
"""Windows-friendly live NumWorks emulator — responsive single-file calculator skin.

Put this file beside tools/emu.py. Requires Pillow, numpy, unicorn,
pyelftools, tkinterdnd2 and nwlink. Displays the NumWorks keyboard artwork
from the locally installed k-kuroguro VS Code extension (CC BY-NC-SA).

Runs an ARM .nwa through the project's existing emu.Calculator, shows the
320x240 display, supports 46 clickable keys, keyboard, .nwa drops and reload.

To run:
    python tools/emu_live.py path/to/NumPlay.nwa

The calculator skin does not depend on emu_live_skin.py; emu.py is still needed.
V10 adds firmware text fallback and corrects off-screen display rectangle clipping.
V9 keeps the V8 virtual-time SVC yields and optimizes full-width display transfers.
An optional --profile-speed report distinguishes host CPU bottlenecks from pacing.
Without SVC yields,
Unicorn can execute hundreds of 16ms sleeps in one 1ms instruction-count batch,
leaping seconds into the future before the GUI can process input.
"""
from __future__ import annotations

import argparse
import os
import queue
import shutil
import struct
import sys
import threading
import time
import traceback
import webbrowser
from pathlib import Path
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

import numpy as np
from PIL import Image, ImageTk, ImageDraw, ImageFont

try:
    from tkinterdnd2 import DND_FILES, TkinterDnD
except ImportError:
    DND_FILES = None
    TkinterDnD = None

# emu.py is a sibling file when installed in the repository's tools folder.
try:
    import emu
except ImportError as exc:
    raise SystemExit("Put emu_live.py inside NumPlay/tools/, beside emu.py. "
                     f"Import failed: {exc}") from exc

REPO_ROOT = Path(__file__).resolve().parent.parent
HOME_NWA = Path.home() / "Downloads" / "nwa"
SAVE_DIR = Path.home() / ".numplay-emulator" / "saves"
FRAME_VIRTUAL_MS = 32
STEP_VIRTUAL_MS = 2  # Bound application execution between worker checks.
UI_POLL_MS = 45
WATCH_MS = 1000
BG = "#151b24"
PANEL = "#222c3b"
PANEL_2 = "#2b3849"
FG = "#f5f7fa"
MUTED = "#abb8c9"
ACCENT = "#64d7c9"

# All 46 named keys in emu.KEYS. Blank cells below are intentional spacers.
KEY_ROWS = [
    ["home", "back", "onoff", "shift", "alpha", "backspace"],
    ["xnt", "var", "toolbox", "up", "ok", "exe"],
    ["exp", "ln", "log", "left", "down", "right"],
    ["i", "comma", "power", "sin", "cos", "tan"],
    ["pi", "sqrt", "square", "lparen", "rparen", None],
    ["7", "8", "9", "div", "mul", "minus"],
    ["4", "5", "6", "plus", "ee", "ans"],
    ["1", "2", "3", "0", "dot", None],
]
LABEL = {
    "home": "Home", "back": "Back", "onoff": "Power", "shift": "Shift",
    "alpha": "Alpha", "backspace": "⌫", "xnt": "x,n,t", "var": "var",
    "toolbox": "Toolbox", "up": "↑", "down": "↓", "left": "←", "right": "→",
    "ok": "OK", "exe": "EXE", "exp": "eˣ", "ln": "ln", "log": "log",
    "i": "i", "comma": ",", "power": "^", "sin": "sin", "cos": "cos",
    "tan": "tan", "pi": "π", "sqrt": "√", "square": "x²",
    "lparen": "(", "rparen": ")", "div": "÷", "mul": "×",
    "minus": "−", "plus": "+", "ee": "×10ⁿ", "ans": "ans", "dot": ".",
}

# Every calculator function can also be clicked on the on-screen keyboard.
# PC bindings emphasize unambiguous shortcuts: F1-F12 + familiar keys.
PC_KEYSYM = {
    "left": "left", "right": "right", "up": "up", "down": "down",
    "return": "ok", "kp_enter": "exe", "escape": "back", "home": "home",
    "delete": "backspace", "backspace": "backspace", "shift_l": "shift", "shift_r": "shift",
    "alt_l": "alpha", "alt_r": "alpha", "space": "ok",
    "f1": "home", "f2": "back", "f3": "shift", "f4": "alpha",
    "f5": "toolbox", "f6": "var", "f7": "xnt", "f8": "onoff",
    "f9": "ans", "f10": "exe", "f11": "sqrt", "f12": "square",
    "plus": "plus", "kp_add": "plus", "minus": "minus", "kp_subtract": "minus",
    "asterisk": "mul", "kp_multiply": "mul", "slash": "div", "kp_divide": "div",
    "period": "dot", "kp_decimal": "dot", "comma": "comma", "parenleft": "lparen",
    "parenright": "rparen", "asciicircum": "power",
}
PC_KEYSYM.update({str(n): str(n) for n in range(10)})
PC_KEYSYM.update({f"kp_{n}": str(n) for n in range(10)})
PC_KEYSYM.update({
    "x": "xnt", "v": "var", "b": "toolbox", "e": "exp", "l": "ln",
    "g": "log", "i": "i", "s": "sin", "c": "cos", "t": "tan",
    "p": "pi", "q": "sqrt", "w": "square", "r": "power", "n": "ee",
    "a": "ans",
})


def validate_layout():
    placed = [k for row in KEY_ROWS for k in row if k is not None]
    if set(placed) != set(emu.KEYS) or len(placed) != len(set(placed)):
        missing = set(emu.KEYS) - set(placed)
        extras = set(placed) - set(emu.KEYS)
        raise RuntimeError(f"Keypad does not match emu.KEYS: missing={missing}, extras={extras}")


def find_nwas() -> list[Path]:
    """Quick bounded discovery: local builds and the user's normal download folder."""
    patterns = (
        REPO_ROOT / "dist" / "live" / "*.nwa",
        REPO_ROOT / "dist" / "*.nwa",
        REPO_ROOT / "dist" / "games" / "*.nwa",
        REPO_ROOT / "dist" / "variants" / "*.nwa",
        REPO_ROOT / "dist" / "languages" / "*.nwa",
        REPO_ROOT / "build" / "*.nwa",
        REPO_ROOT / "build" / "apps" / "*.nwa",
        REPO_ROOT / "games" / "*" / "output" / "*.nwa",
        REPO_ROOT / "games" / "*" / "build" / "*.nwa",
        HOME_NWA / "*.nwa",
        HOME_NWA / "apps" / "*.nwa",
    )
    # Path.glob does not accept absolute pattern strings; use glob.glob.
    import glob
    found: dict[str, Path] = {}
    for pattern in patterns:
        for name in glob.glob(str(pattern)):
            path = Path(name).resolve()
            if path.is_file():
                found[str(path).lower()] = path
    return sorted(found.values(), key=lambda p: p.stat().st_mtime_ns, reverse=True)


def nwlink_command() -> str | None:
    if os.name == "nt":
        return shutil.which("nwlink.cmd") or shutil.which("nwlink.exe")
    return shutil.which("nwlink")


def fingerprint(path: Path) -> tuple[int, int] | None:
    try:
        s = path.stat()
        return (s.st_size, s.st_mtime_ns)
    except OSError:
        return None


def rgb565_to_photo_bytes(pixels: bytes) -> Image.Image:
    a = np.frombuffer(pixels, dtype="<u2").reshape(240, 320)
    r = ((a >> 11) & 31) * 255 // 31
    g = ((a >> 5) & 63) * 255 // 63
    b = (a & 31) * 255 // 31
    rgb = np.stack((r, g, b), axis=-1).astype(np.uint8)
    return Image.fromarray(rgb, "RGB")


class FidelityFont:
    """Draw EADK firmware text with real glyphs, even without EPSILON_FONTS.

    emu.Font used empty glyph dictionaries whenever EPSILON_FONTS was unset,
    and only populated ASCII 32..126 when it was set. That rendered valid
    firmware draw_string calls as blank foreground-free rectangles. We prefer
    locally supplied Epsilon TTFs and use installed system fonts otherwise.
    No fonts are downloaded or redistributed.
    """

    def __init__(self):
        configured = Path(emu.FONT_DIR).expanduser() if emu.FONT_DIR else None
        repo_fonts = (REPO_ROOT / "tools" / "fonts", REPO_ROOT / "fonts",
                      Path.home() / ".numplay-emulator" / "fonts")
        system_fonts = (
            Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts" / "segoeui.ttf",
            Path(os.environ.get("WINDIR", r"C:\Windows")) / "Fonts" / "arial.ttf",
            Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
        )
        self.cells = {}
        self.cache = {}
        self.sources = []
        for large, (filename, size, width, height) in (
            (False, ("SmallFont.ttf", 12, 7, 14)),
            (True, ("LargeFont.ttf", 16, 10, 18)),
        ):
            candidates = []
            if configured:
                candidates.append(configured / filename)
            candidates.extend(folder / filename for folder in repo_fonts)
            candidates.extend(system_fonts)
            # Pillow ships/locates DejaVu Sans on most installations. Prefer
            # this over the 1-bit PIL fallback if no system font can be found.
            candidates.append("DejaVuSans.ttf")
            font = None
            source = None
            for location in candidates:
                try:
                    font = ImageFont.truetype(str(location), size)
                    source = str(location)
                    break
                except (OSError, ValueError):
                    pass
            if font is None:
                try:
                    font = ImageFont.load_default(size=size)
                except TypeError:
                    font = ImageFont.load_default()
                source = "Pillow default fallback"
            self.cells[large] = (width, height, font)
            self.cache[large] = {}
            self.sources.append(f"{'large' if large else 'small'}: {source}")
        print("[NumPlay fonts] " + " | ".join(self.sources), file=sys.stderr, flush=True)
        if not configured:
            print("[NumPlay fonts] Exact Epsilon fonts are optional: set EPSILON_FONTS "
                  "to a directory containing SmallFont.ttf and LargeFont.ttf.",
                  file=sys.stderr, flush=True)

    def _glyph(self, char: str, large: bool) -> bytes:
        cache = self.cache[large]
        if char not in cache:
            w, h, f = self.cells[large]
            img = Image.new("L", (w, h), 0)
            asc = f.getmetrics()[0] if hasattr(f, "getmetrics") else h - 2
            # Maintain the baseline used by emu.Font; match fixed EADK cells.
            y = h - 3 - asc + (1 if large else 0)
            if char not in ("\n", "\r", "\t"):
                ImageDraw.Draw(img).text((0, y), char, font=f, fill=255)
            if len(cache) >= 512:
                cache.clear()
            cache[char] = img.tobytes()
        return cache[char]

    def draw(self, screen: bytearray, text: str, x: int, y: int,
             large: bool, fg: int, bg: int) -> int:
        w, h, _ = self.cells[large]
        fg &= 0xffff
        bg &= 0xffff
        # Precalculate the 256 RGB565 alpha blends once per call, not once
        # per pixel; retain the backend's own interpolation semantics.
        palette = []
        for t in range(256):
            out = 0
            for shift, mask in ((11, 31), (5, 63), (0, 31)):
                a = (bg >> shift) & mask
                b = (fg >> shift) & mask
                out |= ((a * (255 - t) + b * t) // 255) << shift
            palette.append(struct.pack("<H", out))
        cx = int(x)
        for char in text:
            if cx >= 320:
                break
            x0, x1 = max(0, -cx), min(w, 320 - cx)
            y0, y1 = max(0, -y), min(h, 240 - y)
            if x0 < x1 and y0 < y1:
                glyph = self._glyph(char, large)
                for j in range(y0, y1):
                    beg = j * w + x0
                    end = j * w + x1
                    dst = ((y + j) * 320 + cx + x0) * 2
                    screen[dst:dst + (end - beg) * 2] = b"".join(palette[v] for v in glyph[beg:end])
            cx += w
        return cx


class TracedCalculator(emu.Calculator):
    """Keep the headless emulator intact, but yield on time-changing syscalls.

    In emu.py, Calculator.run() requests a full millisecond of ARM *instructions*
    at once. A program can call eadk_timing_msleep(16) hundreds of times before
    that instruction budget expires. Each call immediately advances now_ms, so
    a single emu_start() can advance several *seconds* of virtual time. A 2ms
    outer frontend slice cannot interrupt that work.

    End the current Unicorn batch when a system call advances virtual time.
    Calculator.run() then checks its until_ms limit and the frontend gets back
    control between ordinary app sleeps, preserving keyboard responsiveness.
    """

    # EADK event_get with no key (23), flash erase/write (30/32), and the
    # millisecond/microsecond sleep calls (49/50) can advance emulated time.
    TIME_ADVANCING_SVCS = (23, 30, 32, 49, 50)

    def __init__(self, *args, max_sleep_ms: int = 0, **kwargs):
        self.max_sleep_ms = max_sleep_ms
        self.last_long_sleep = None   # (requested_ms, used_ms, svc, pc, symbol)
        self.long_sleep_count = 0
        self.last_svc_yield = None    # (svc, pc, requested_or_delta_ms)
        self.svc_yield_count = 0
        self._sleep_warning_at = {}  # SVC site and duration -> last diagnostic timestamp
        # NumDrive draws 320-pixel-wide strips. Record completed bottom strips
        # as an estimate of actual game frames, independent of UI refreshes.
        self.fullwidth_blits = 0
        self.fullwidth_bytes = 0
        self.complete_display_frames = 0
        self.fullwidth_copy_seconds = 0.0
        super().__init__(*args, **kwargs)
        self.font = FidelityFont()
        self.clipped_display_rects = 0

    def _fast_fullwidth_display(self, uc, svc: int) -> bool:
        """Byte-copy a whole screen-width strip in one operation.

        The original emu.Calculator._intr handles SVC 18/19/20 correctly, but
        copies every row individually in Python. NumDrive's 3D renderer calls
        push_rect(0, strip_y, 320, strip_h) for every rendered strip. Such
        strips are contiguous in *both* source and framebuffer and can be
        transferred exactly with a single Unicorn read/write and slice.
        All partial-width and out-of-bounds rectangles use the original path.
        """
        r0 = uc.reg_read(emu.UC_ARM_REG_R0)
        r1 = uc.reg_read(emu.UC_ARM_REG_R1)
        x, y = r0 & 0xFFFF, r0 >> 16
        w, h = r1 & 0xFFFF, r1 >> 16
        if x != 0 or w != 320 or h == 0 or y + h > 240:
            return False
        started = time.perf_counter()
        offset = y * 320 * 2
        length = w * h * 2
        if svc == 19:   # eadk_display_push_rect
            self.screen[offset:offset + length] = uc.mem_read(
                uc.reg_read(emu.UC_ARM_REG_R2), length)
            if y + h == 240:
                self.complete_display_frames += 1
        elif svc == 18:  # eadk_display_pull_rect
            uc.mem_write(uc.reg_read(emu.UC_ARM_REG_R2),
                         bytes(self.screen[offset:offset + length]))
        elif svc == 20:  # eadk_display_push_rect_uniform
            color = uc.reg_read(emu.UC_ARM_REG_R2) & 0xFFFF
            self.screen[offset:offset + length] = color.to_bytes(2, "little") * (w * h)
        else:
            return False
        # Match the backend's accounting. Do not suppress an EADK syscall.
        self.svc_counts[svc] = self.svc_counts.get(svc, 0) + 1
        self.insns += w * h // 4
        self.fullwidth_blits += 1
        self.fullwidth_bytes += length
        self.fullwidth_copy_seconds += time.perf_counter() - started
        return True

    def _draw_string(self, uc, addr, size, data):
        """Render firmware EADK strings; include Unicode and longer labels.

        The emulator base class read 256 bytes regardless of the string's
        length and silently blanked any non-ASCII glyphs. Respect the NUL
        terminator while limiting input to 2 KiB for a faulty application.
        """
        regs = (emu.UC_ARM_REG_R0, emu.UC_ARM_REG_R1,
                emu.UC_ARM_REG_R2, emu.UC_ARM_REG_R3)
        ptr, point, large, fg = (uc.reg_read(r) for r in regs)
        sp = uc.reg_read(emu.UC_ARM_REG_SP)
        bg = int.from_bytes(uc.mem_read(sp, 2), "little")
        raw = bytearray()
        for offset in range(0, 2048, 32):
            block = bytes(uc.mem_read(ptr + offset, 32))
            nul = block.find(b"\x00")
            raw.extend(block if nul < 0 else block[:nul])
            if nul >= 0:
                break
        text = raw.decode("utf-8", "replace")
        self.font.draw(self.screen, text, point & 0xFFFF, point >> 16,
                       bool(large & 0xFF), fg & 0xFFFF, bg)
        uc.reg_write(emu.UC_ARM_REG_PC, uc.reg_read(emu.UC_ARM_REG_LR))

    def _clipped_display(self, uc, svc: int) -> bool:
        """Keep original input-buffer row stride when an EADK rect is clipped.

        The backend reduces w before stepping across source rows. With a
        clipped rectangle, that mistakenly pulls pixels from the next row,
        looking like wrong colors/corrupt graphics at the screen edges.
        """
        r0 = uc.reg_read(emu.UC_ARM_REG_R0)
        r1 = uc.reg_read(emu.UC_ARM_REG_R1)
        x, y = r0 & 0xFFFF, r0 >> 16
        src_w, src_h = r1 & 0xFFFF, r1 >> 16
        if x + src_w <= 320 and y + src_h <= 240:
            return False   # fast path or the backend already handles this.
        self.clipped_display_rects += 1
        if self.clipped_display_rects <= 4:
            print(f"[NumPlay display] Clipped SVC {svc}: rect "
                  f"({x}, {y}, {src_w}, {src_h})", file=sys.stderr)
        if x >= 320 or y >= 240 or src_w == 0 or src_h == 0:
            self.svc_counts[svc] = self.svc_counts.get(svc, 0) + 1
            return True
        w, h = min(src_w, 320 - x), min(src_h, 240 - y)
        pointer = uc.reg_read(emu.UC_ARM_REG_R2)
        for row in range(h):
            dst = ((y + row) * 320 + x) * 2
            length = w * 2
            if svc == 19:  # Push rows, preserving the ORIGINAL rectangle width.
                self.screen[dst:dst + length] = uc.mem_read(pointer + row * src_w * 2, length)
            elif svc == 18:  # Pull rows, likewise preserve destination stride.
                uc.mem_write(pointer + row * src_w * 2, bytes(self.screen[dst:dst + length]))
            elif svc == 20:
                color = pointer & 0xFFFF
                self.screen[dst:dst + length] = color.to_bytes(2, "little") * w
        self.svc_counts[svc] = self.svc_counts.get(svc, 0) + 1
        self.insns += w * h // 4
        return True

    def _intr(self, uc, intno, data):
        # The backend registers self._intr during Calculator.__init__.
        if intno != 2:
            return super()._intr(uc, intno, data)
        pc = uc.reg_read(emu.UC_ARM_REG_PC)
        op = int.from_bytes(uc.mem_read(pc - 2, 2), "little")
        svc = op & 0xFF
        if svc in (18, 19, 20):
            if self._fast_fullwidth_display(uc, svc) or self._clipped_display(uc, svc):
                return
        start_ms = float(self.now_ms)
        requested_ms = None
        used_ms = None
        if svc in (49, 50):
            raw = uc.reg_read(emu.UC_ARM_REG_R0)
            requested_ms = float(raw if svc == 49 else raw / 1000.0)
            used_ms = requested_ms
            if self.max_sleep_ms and requested_ms > self.max_sleep_ms:
                limit = self.max_sleep_ms * (1000 if svc == 50 else 1)
                uc.reg_write(emu.UC_ARM_REG_R0, int(limit))
                used_ms = float(self.max_sleep_ms)

        # Run the backend's real SVC implementation: preserves keyboard,
        # flash, display, register returns, and all other NumPlay semantics.
        result = super()._intr(uc, intno, data)
        delta_ms = float(self.now_ms) - start_ms

        if requested_ms is not None:
            if requested_ms >= 250 or used_ms != requested_ms:
                where = pc - 2
                symbol = "(stripped/unknown symbol)"
                for begin, end, name in getattr(self, "functions", ()):
                    if begin <= where < end:
                        symbol = name
                        break
                self.last_long_sleep = (requested_ms, used_ms, svc, where, symbol)
                self.long_sleep_count += 1
            # Trace normal 16ms calls too! V7 only traced >=250ms,
            # which is why sleep_svc=None even for a 3808ms step.
            if delta_ms >= 1:
                signature = (svc, pc - 2, round(requested_ms, 1))
                now = time.monotonic()
                if now - self._sleep_warning_at.get(signature, -1e9) >= 30:
                    self._sleep_warning_at[signature] = now
                    extra = (f" (guarded to {used_ms:.0f}ms)"
                             if used_ms != requested_ms else "")
                    print(f"[NumPlay sleep] svc={svc} PC=0x{pc-2:08x}; "
                          f"requested={requested_ms:.1f}ms{extra}; "
                          "yielding at SVC boundary",
                          file=sys.stderr, flush=True)

        if svc == 23 and delta_ms >= 1:
            # emu.py's 'no event' path adds 10ms, but does not mark the
            # interval as idle. Correct the worker's CPU-load measurement.
            self.slept_ms += delta_ms

        if svc in self.TIME_ADVANCING_SVCS and delta_ms >= 0.5:
            self.last_svc_yield = (svc, pc - 2, delta_ms)
            self.svc_yield_count += 1
            # emu.py unconditionally adds +1ms after emu_start() returns,
            # assuming its entire instruction budget ran. We deliberately
            # stopped before the budget was exhausted. Cancel that extra ms
            # now; the backend adds it back immediately after emu_stop().
            # (CPU cycles spent before the SVC remain an approximation.)
            self.now_ms -= 1.0
            uc.emu_stop()
        return result


class EmulatorWorker(threading.Thread):
    def __init__(self, app_path: Path, nwlink: str, events: queue.Queue,
                 stop: threading.Event, paused: threading.Event,
                 pressed: set[str], pressed_lock: threading.Lock, save_dir: Path,
                 model: str = "n0120", system: str = "epsilon",
                 clock_mhz: list[int] | None = None,
                 auto_clock: list[bool] | None = None,
                 max_sleep_ms: int = 0,
                 profile_speed: bool = False):
        super().__init__(daemon=True)
        self.app_path, self.nwlink, self.events = app_path, nwlink, events
        self.stop_requested, self.paused = stop, paused
        self.pressed, self.pressed_lock = pressed, pressed_lock
        self.save_dir = save_dir
        self.model, self.system = model, system
        self.max_sleep_ms = max_sleep_ms
        self.profile_speed = profile_speed
        # The UI may change this list's sole int while the worker runs.
        # Access is atomic under Python's GIL; only this worker calls emu.run().
        self.clock_mhz = clock_mhz if clock_mhz is not None else [216]
        self.auto_clock = auto_clock if auto_clock is not None else [True]
        self.end_reason: str | None = None
        self.last_sleep_delta_ms = 0.0
        self.last_svc_detail = None
        self.last_sleep_detail = None
        # Plain Python attributes: GUI only reads them; the worker only writes.
        # These distinguish an overloaded Unicorn call from a legitimate EADK sleep.
        self.phase = "initializing"
        self.phase_since_wall = time.monotonic()
        self.last_progress_wall = self.phase_since_wall
        self.last_progress_ms = 0.0
        self.last_cpu_seconds = 0.0
        self.last_virtual_delta = 0.0

    def set_phase(self, name: str):
        if name != self.phase:
            self.phase = name
            self.phase_since_wall = time.monotonic()

    def emit(self, kind: str, *payload):
        if kind == "frame":
            try:
                self.events.put_nowait((kind, *payload))
            except queue.Full:
                # Drop an old screen rather than lagging behind the real app.
                try:
                    self.events.get_nowait()
                except queue.Empty:
                    pass
                try:
                    self.events.put_nowait((kind, *payload))
                except queue.Full:
                    pass
        else:
            # A small event queue is enough, even if frame production is fast.
            while True:
                try:
                    self.events.put_nowait((kind, *payload))
                    break
                except queue.Full:
                    try:
                        self.events.get_nowait()
                    except queue.Empty:
                        pass

    def storage_path(self) -> Path:
        # Keep saves for separate .nwa names in a stable user directory.
        import hashlib
        unique = hashlib.sha256(str(self.app_path).lower().encode("utf-8")).hexdigest()[:10]
        return self.save_dir / f"{self.app_path.stem}-{unique}.bin"

    def read_keyboard(self) -> int:
        with self.pressed_lock:
            names = tuple(self.pressed)
        state = 0
        for k in names:
            state |= 1 << emu.KEYS[k]
        return state

    def run(self):
        app = None
        save_path = self.storage_path()
        try:
            self.save_dir.mkdir(parents=True, exist_ok=True)
            self.emit("status", f"Linking {self.app_path.name} with nwlink…")
            emu.configure(self.model, self.system)
            app = TracedCalculator(str(self.app_path), storage_file=str(save_path),
                                   nwlink=self.nwlink, max_sleep_ms=self.max_sleep_ms)
            # Avoid rewriting a save file 30 times per second; save on stop.
            app.storage_file = None
            app.keyboard_state = self.read_keyboard
            self.emit("status", f"Running {self.app_path.name}")
            # Run ARM instructions in *small* virtual-time slices. A full 33ms
            # emu.Calculator.run() was blocking the only CPU worker for seconds
            # in expensive games and at module transitions. A 2ms slice limits
            # the time before the next keyboard/frame/stop check. It preserves
            # the backend's instruction-count and EADK msleep clock model.
            was_paused = False
            clock_anchor = time.monotonic()
            last_frame_wall = clock_anchor
            last_frame_virtual = float(app.now_ms)
            rate_wall = clock_anchor
            rate_virtual = float(app.now_ms)
            busy_wall = busy_virtual = 0.0
            last_auto_adjust = clock_anchor
            smoothed_load = None
            self.last_progress_wall = clock_anchor
            self.last_progress_ms = float(app.now_ms)
            profile_start = clock_anchor
            profile_start_virtual = float(app.now_ms)
            profile_start_frames = app.complete_display_frames
            profile_start_blits = app.fullwidth_blits
            profile_start_copy_seconds = app.fullwidth_copy_seconds
            profile_cpu_seconds = 0.0
            profile_wait_seconds = 0.0
            while not self.stop_requested.is_set() and not app.exited:
                if self.paused.is_set():
                    self.set_phase("paused")
                    was_paused = True
                    self.stop_requested.wait(0.035)
                    continue
                if was_paused:
                    # Re-anchor; never run faster to pay back paused/late frames.
                    clock_anchor = time.monotonic()
                    last_auto_adjust = rate_wall = clock_anchor
                    rate_virtual = float(app.now_ms)
                    busy_wall = busy_virtual = 0.0
                    smoothed_load = None
                    was_paused = False
                    profile_start = clock_anchor
                    profile_start_virtual = float(app.now_ms)
                    profile_start_frames = app.complete_display_frames
                    profile_start_blits = app.fullwidth_blits
                    profile_start_copy_seconds = app.fullwidth_copy_seconds
                    profile_cpu_seconds = profile_wait_seconds = 0.0

                emu.CPU_HZ = max(27, min(216, int(self.clock_mhz[0]))) * 1_000_000
                started = time.monotonic()
                self.set_phase("running ARM CPU")
                before_ms = float(app.now_ms)
                before_sleep_ms = float(app.slept_ms)
                before_sleep_count = app.long_sleep_count
                before_yield_count = app.svc_yield_count
                app.run(app.now_ms + STEP_VIRTUAL_MS)
                work_seconds = max(0.0, time.monotonic() - started)
                profile_cpu_seconds += work_seconds
                delta_ms = max(0.0, float(app.now_ms) - before_ms)
                slept_ms = max(0.0, float(app.slept_ms) - before_sleep_ms)
                active_ms = max(0.0, delta_ms - min(delta_ms, slept_ms))
                self.last_cpu_seconds = work_seconds
                self.last_virtual_delta = delta_ms
                self.last_sleep_delta_ms = slept_ms
                self.last_sleep_detail = (app.last_long_sleep
                                          if app.long_sleep_count != before_sleep_count else None)
                self.last_svc_detail = (app.last_svc_yield
                                        if app.svc_yield_count != before_yield_count else None)
                self.last_progress_wall = time.monotonic()
                self.last_progress_ms = float(app.now_ms)
                if self.stop_requested.is_set() or app.exited:
                    break

                # A large emulated sleep is not evidence that the CPU is slow.
                # In particular, don't downclock because a single emulated
                # millisecond of ARM work accompanied a six-second SVC sleep.
                if self.auto_clock[0] and active_ms >= 0.5 and slept_ms < 250:
                    busy_wall += work_seconds
                    busy_virtual += active_ms

                # Present a frame BEFORE wall-clock pacing; a long sleep should
                # not hide a screen update that the app already performed.
                wall_now = time.monotonic()
                if (float(app.now_ms) - last_frame_virtual >= FRAME_VIRTUAL_MS or
                        wall_now - last_frame_wall >= 0.12):
                    self.emit("frame", bytes(app.screen), int(app.now_ms))
                    last_frame_wall = wall_now
                    last_frame_virtual = float(app.now_ms)

                # One simulated millisecond must never take *less* than one
                # actual millisecond. Sleep overestimated virtual time, but
                # stop waiting as soon as Reload, Pause or Power is requested.
                # Keep a continuous timeline when on time; erase lateness
                # beyond 40ms so there are NO catch-up bursts.
                clock_anchor = max(clock_anchor, started - 0.040)
                clock_anchor += delta_ms / 1000.0
                wait_started = time.monotonic()
                while not self.stop_requested.is_set() and not self.paused.is_set():
                    remaining = clock_anchor - time.monotonic()
                    if remaining <= 0:
                        break
                    self.set_phase("waiting for app sleep" if slept_ms >= 250
                                   else "waiting for emulated time")
                    self.stop_requested.wait(min(remaining, 0.015))
                profile_wait_seconds += max(0.0, time.monotonic() - wait_started)
                if self.stop_requested.is_set():
                    break
                if self.paused.is_set():
                    was_paused = True
                    continue

                self.set_phase("running ARM CPU")
                wall_now = time.monotonic()
                if self.profile_speed and wall_now - profile_start >= 5.0:
                    seconds = wall_now - profile_start
                    frames = app.complete_display_frames - profile_start_frames
                    blits = app.fullwidth_blits - profile_start_blits
                    simulated = float(app.now_ms) - profile_start_virtual
                    # One full-width bottom strip generally completes one
                    # NumDrive 3D image. Report graphics throughput *separately*
                    # from how quickly the emulated virtual clock advances.
                    copied_ms = (app.fullwidth_copy_seconds -
                                 profile_start_copy_seconds) * 1000.0
                    avg_frame_ms = (f"{1000*profile_cpu_seconds/frames:.1f}"
                                    if frames else "n/a")
                    print(f"[NumPlay speed profile] "
                          f"render_fps~{frames/seconds:.1f}; "
                          f"virtual_rate={simulated/(1000*seconds):.2f}x; "
                          f"ARM_work={100*profile_cpu_seconds/seconds:.0f}%; "
                          f"wall_wait={100*profile_wait_seconds/seconds:.0f}%; "
                          f"CPU={int(self.clock_mhz[0])}MHz "
                          f"({'auto' if self.auto_clock[0] else 'manual'}); "
                          f"fullwidth_blits={blits}; "
                          f"copy_ms={copied_ms:.1f}; "
                          f"avg_ARM_ms_per_frame={avg_frame_ms}",
                          file=sys.stderr, flush=True)
                    profile_start = wall_now
                    profile_start_virtual = float(app.now_ms)
                    profile_start_frames = app.complete_display_frames
                    profile_start_blits = app.fullwidth_blits
                    profile_start_copy_seconds = app.fullwidth_copy_seconds
                    profile_cpu_seconds = profile_wait_seconds = 0.0
                # Calibrate based on actual Unicorn throughput, separately
                # from time spent inside EADK sleep. Earlier 1.25s sampling
                # intervals could take far longer in a busy scene. Faster
                # sampling and bounded steps make this responsive without a
                # constantly oscillating MHz value.
                if self.auto_clock[0] and wall_now - last_auto_adjust >= 0.45:
                    if busy_virtual >= 8.0:
                        load = busy_wall / max(0.001, busy_virtual / 1000.0)
                        smoothed_load = (load if smoothed_load is None else
                                         0.65 * smoothed_load + 0.35 * load)
                        old = int(self.clock_mhz[0])
                        if smoothed_load > 1.05:
                            new = max(27, int(old * 0.80))
                        elif smoothed_load < 0.58 and old < 216:
                            new = min(216, max(old + 1, int(old * 1.05)))
                        else:
                            new = old
                        self.clock_mhz[0] = new
                    busy_wall = busy_virtual = 0.0
                    last_auto_adjust = wall_now
                elif not self.auto_clock[0]:
                    busy_wall = busy_virtual = 0.0
                    smoothed_load = None
                    last_auto_adjust = wall_now

                if wall_now - rate_wall >= 1.0:
                    rate = (float(app.now_ms) - rate_virtual) / (1000.0 * (wall_now - rate_wall))
                    self.emit("pace", round(rate, 2), int(self.clock_mhz[0]), bool(self.auto_clock[0]))
                    rate_wall, rate_virtual = wall_now, float(app.now_ms)
            if app.exited:
                self.set_phase("exited")
                # The caller must blank the screen; the final framebuffer is
                # NOT a valid representation of a powered-off calculator.
                self.end_reason = "exited"
                self.emit("ended", "Application exited — press Power or Reload to restart")
        except Exception:
            self.set_phase("error")
            self.emit("error", traceback.format_exc())
        finally:
            if not app or not app.exited:
                if self.phase != "error":
                    self.set_phase("stopped")
            if app is not None:
                try:
                    save_path.write_bytes(app.storage_bytes())
                except Exception as exc:
                    self.emit("error", f"Could not save emulated storage:\n{exc}")


class LiveApp:
    def __init__(self, root: tk.Tk, initial: Path | None, nwlink: str,
                 model: str, system: str, auto_follow: bool,
                 max_sleep_ms: int = 0, profile_speed: bool = False):
        validate_layout()
        self.root, self.nwlink = root, nwlink
        self.model, self.system = model, system
        self.max_sleep_ms = max_sleep_ms
        self.profile_speed = profile_speed
        self.clock_mhz = [216]   # Shared with the worker. Auto-controller updates this.
        self.auto_clock = [True] # Can be toggled from the skinned sidebar.
        self.powered_off = False
        self._power_key_held = False
        self.root.title("NumPlay Live Emulator")
        self.root.configure(bg=BG)
        self.root.minsize(690, 820)
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)
        self.worker: EmulatorWorker | None = None
        self.stop_event = threading.Event()
        self.pause_event = threading.Event()
        self.messages: queue.Queue = queue.Queue(maxsize=3)
        self.pressed: set[str] = set()
        self.pressed_pc: dict[str, str] = {}
        self.mouse_pressed: str | None = None
        self.pressed_lock = threading.Lock()
        self.key_widgets: dict[str, tk.Label] = {}
        self.current: Path | None = None
        self.pending: Path | None = None
        self.current_stamp: tuple[int, int] | None = None
        self.watch_candidate: tuple[tuple[str, tuple[int, int]], int] | None = None
        self.watch_var = tk.BooleanVar(value=True)
        self.follow_var = tk.BooleanVar(value=auto_follow)
        self.status = tk.StringVar(value="Ready — drop or open a .nwa")
        self.file_label = tk.StringVar(value="No application loaded")
        self.time_label = tk.StringVar(value="0 ms")
        self._screen_rgb: Image.Image | None = None
        self._photo = None
        self._closing = False
        self._last_frame_wall = time.monotonic()
        self._last_stall_log_wall = 0.0
        self.input_status = tk.StringVar(value="INPUT  •  ready")
        self._build_ui()
        self.root.bind_all("<KeyPress>", self.on_pc_down, add="+")
        self.root.bind_all("<KeyRelease>", self.on_pc_up, add="+")
        self.root.bind_all("<ButtonRelease-1>", self.on_mouse_up, add="+")
        self.root.bind("<FocusOut>", self.on_focus_out, add="+")
        self.root.bind("<FocusIn>", self.on_focus_in, add="+")
        self.root.bind("<Map>", self.on_window_map, add="+")
        if DND_FILES is not None:
            for widget in (self.root, self.screen_label, self.drop_line):
                widget.drop_target_register(DND_FILES)
                widget.dnd_bind("<<Drop>>", self.on_drop)
        self._poll_id = self.root.after(UI_POLL_MS, self.poll)
        self._watch_id = self.root.after(WATCH_MS, self.watch)
        if initial is not None:
            self.root.after(150, lambda: self.load(initial))
        else:
            latest = find_nwas()
            if latest:
                self.root.after(150, lambda: self.load(latest[0]))
            else:
                self.root.after(250, self.open_file)

    def _build_ui(self):
        top = tk.Frame(self.root, bg=BG)
        top.pack(fill="x", padx=12, pady=(12, 4))
        tk.Label(top, text="NumPlay  •  Live Emulator", bg=BG, fg=FG,
                 font=("Segoe UI", 17, "bold")).pack(side="left")
        tk.Label(top, textvariable=self.time_label, bg=BG, fg=MUTED,
                 font=("Segoe UI", 10)).pack(side="right")
        actions = tk.Frame(self.root, bg=BG)
        actions.pack(fill="x", padx=10, pady=6)
        for title, cb in (("Open .nwa", self.open_file), ("Find newest", self.find_latest),
                          ("Reload", self.reset), ("Pause", self.toggle_pause),
                          ("Screenshot", self.screenshot), ("Keymap", self.show_keymap)):
            button = tk.Button(actions, text=title, command=cb, bg=PANEL_2, fg=FG,
                               activebackground=ACCENT, activeforeground=BG,
                               relief="flat", bd=0, padx=9, pady=7, font=("Segoe UI", 9))
            button.pack(side="left", padx=2)
            if title == "Pause":
                self.pause_button = button
        choices = tk.Frame(self.root, bg=BG)
        choices.pack(fill="x", padx=12, pady=(0, 6))
        for var, label in ((self.watch_var, "Auto-reload changed .nwa"),
                           (self.follow_var, "Follow newest local build")):
            tk.Checkbutton(choices, text=label, variable=var, bg=BG, fg=MUTED,
                           activebackground=BG, activeforeground=ACCENT,
                           selectcolor=PANEL, font=("Segoe UI", 9)).pack(side="left", padx=4)
        path_panel = tk.Frame(self.root, bg=PANEL, padx=10, pady=7)
        path_panel.pack(fill="x", padx=12, pady=2)
        tk.Label(path_panel, textvariable=self.file_label, bg=PANEL, fg=FG,
                 anchor="w", font=("Segoe UI", 9)).pack(fill="x")
        center = tk.Frame(self.root, bg=BG)
        center.pack(fill="x", padx=12, pady=8)
        self._screen_rgb = Image.new("RGB", (320, 240), (235, 235, 235))
        self._photo = ImageTk.PhotoImage(self._screen_rgb.resize((640, 480), Image.Resampling.NEAREST))
        self.screen_label = tk.Label(center, image=self._photo, bg="#050505", bd=7, relief="flat")
        self.screen_label.pack(anchor="center")
        self.drop_line = tk.Label(center, text=("Drop .nwa here • " if DND_FILES else "") +
                                  "click a key or use your PC keyboard", bg=BG, fg=MUTED,
                                  font=("Segoe UI", 9), pady=4)
        self.drop_line.pack()
        keyboard = tk.Frame(self.root, bg=BG)
        keyboard.pack(fill="both", expand=True, padx=12, pady=5)
        for col in range(6):
            keyboard.grid_columnconfigure(col, weight=1, uniform="numworks")
        for i, row in enumerate(KEY_ROWS):
            keyboard.grid_rowconfigure(i, weight=1)
            for j, key in enumerate(row):
                if key is None:
                    continue
                bg = "#325e68" if key in ("ok", "exe") else ("#36495c" if key in
                        ("left", "right", "up", "down") else PANEL_2)
                if key in ("home", "onoff", "shift", "alpha"):
                    bg = "#3a4c5a"
                w = tk.Label(keyboard, text=LABEL.get(key, key), bg=bg, fg=FG,
                             relief="flat", bd=0, font=("Segoe UI", 10, "bold"),
                             padx=5, pady=5, cursor="hand2")
                w.grid(row=i, column=j, sticky="nsew", padx=3, pady=3)
                w.bind("<ButtonPress-1>", lambda e, k=key: self.on_mouse_down(k))
                w.bind("<ButtonRelease-1>", self.on_mouse_up)
                self.key_widgets[key] = w
        status = tk.Label(self.root, textvariable=self.status, bg=PANEL, fg=ACCENT,
                          anchor="w", padx=12, pady=7, font=("Segoe UI", 9))
        status.pack(fill="x", side="bottom")
        self.root.focus_force()

    def set_status(self, msg: str):
        self.status.set(msg)

    def open_file(self):
        path = filedialog.askopenfilename(parent=self.root, title="Open a NumWorks application",
                                          filetypes=[("NumWorks apps", "*.nwa"), ("All files", "*.*")],
                                          initialdir=str(HOME_NWA if HOME_NWA.exists() else REPO_ROOT))
        if path:
            self.follow_var.set(False)
            self.load(Path(path))

    def on_drop(self, event):
        for s in self.root.tk.splitlist(event.data):
            if s.lower().endswith(".nwa"):
                self.follow_var.set(False)
                self.load(Path(s))
                return
        self.set_status("Drop a .nwa file from File Explorer.")

    def find_latest(self):
        found = find_nwas()
        if found:
            self.load(found[0])
        else:
            messagebox.showinfo("No .nwa found", "Build a .nwa or drop one here.\n"
                                "Searches build/, games/*/output/ and Downloads/nwa/.")

    def load(self, path: Path):
        path = path.expanduser().resolve()
        if path.suffix.lower() != ".nwa" or not path.is_file():
            self.set_status(f"Not a valid .nwa file: {path}")
            return
        self.pending = path
        self.pause_event.clear()
        self.pause_button.configure(text="Pause")
        self.clear_keys()
        if self.worker is not None and self.worker.is_alive():
            self.stop_event.set()
            self.set_status(f"Reloading {path.name}…")
        else:
            self.start_pending()

    def start_pending(self):
        path = self.pending
        if path is None:
            return
        self.pending = None
        self.current = path
        self._last_frame_wall = time.monotonic()
        self._last_stall_log_wall = 0.0
        self.current_stamp = fingerprint(path)
        self.watch_candidate = None
        self.file_label.set(str(path))
        self.time_label.set("0 ms")
        self.messages = queue.Queue(maxsize=3)
        self.stop_event = threading.Event()
        self.powered_off = False
        self.worker = EmulatorWorker(path, self.nwlink, self.messages, self.stop_event,
                                     self.pause_event, self.pressed, self.pressed_lock,
                                     SAVE_DIR, self.model, self.system,
                                     self.clock_mhz, self.auto_clock,
                                     self.max_sleep_ms, self.profile_speed)
        self.worker.start()

    def reset(self):
        if self.current:
            self.load(self.current)
        else:
            self.find_latest()

    def present_screen(self):
        """Display the current RGB screen; overridden by the skinned GUI."""
        if self._screen_rgb is None:
            return
        self._photo = ImageTk.PhotoImage(self._screen_rgb.resize((640, 480), Image.Resampling.NEAREST))
        self.screen_label.configure(image=self._photo)

    def black_screen(self, reason: str):
        self.powered_off = True
        self._screen_rgb = Image.new("RGB", (320, 240), (0, 0, 0))
        self.present_screen()
        self.time_label.set("Powered off")
        self.set_status(reason)

    def power_button(self):
        """Power button is a frontend power toggle, not a stuck ARM key.

        Epsilon's power-off firmware isn't emulated. Sending the raw on/off
        key can leave the app waiting forever for firmware wakeup, so blank
        the screen and stop the worker. Click again to boot the .nwa anew.
        """
        if self.powered_off:
            self.reset()
        else:
            self.pending = None
            self.stop_event.set()
            self.pause_event.clear()
            self.pause_button.configure(text="Pause")
            self.clear_keys()
            self.black_screen("Powered off — press Power or Reload to restart")

    def toggle_pause(self):
        if self.pause_event.is_set():
            self.pause_event.clear()
            self.pause_button.configure(text="Pause")
            self.set_status("Resumed")
        else:
            self.pause_event.set()
            self.pause_button.configure(text="Resume")
            self.set_status("Paused (emulated clock stopped)")

    def screenshot(self):
        if self._screen_rgb is None:
            return
        directory = REPO_ROOT / "build" / "emu-live"
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / f"{self.current.stem if self.current else 'screen'}-{int(time.time())}.png"
        self._screen_rgb.save(path)
        self.set_status(f"Screenshot saved: {path}")

    def on_mouse_down(self, key: str):
        if key == "onoff":
            self.power_button()
            return
        if self.powered_off:
            return
        if self.mouse_pressed is not None:
            self.on_mouse_up()
        self.mouse_pressed = key
        self.input_status.set(f"INPUT  •  {key} (mouse)")
        self.refresh_pressed()

    def on_mouse_up(self, event=None):
        if self.mouse_pressed is not None:
            self.mouse_pressed = None
            self.refresh_pressed()

    def on_pc_down(self, event):
        # Don't send keyboard shortcuts from another Toplevel (e.g. Keymap).
        if event.widget.winfo_toplevel() is not self.root:
            return None
        sym = event.keysym.lower()
        key = PC_KEYSYM.get(sym)
        if key == "onoff":
            if not self._power_key_held:
                self._power_key_held = True
                self.power_button()
            return "break"
        if self.powered_off:
            return "break" if key else None
        if key:
            self.pressed_pc[sym] = key
            self.input_status.set(f"INPUT  •  {key} (PC)")
            self.refresh_pressed()
            # ALL NumWorks keys must be intercepted, not just Space. Otherwise
            # Tk buttons/dropdowns can steal arrow keys after a focus change.
            return "break"
        return None

    def on_pc_up(self, event):
        if event.widget.winfo_toplevel() is not self.root:
            return None
        sym = event.keysym.lower()
        if PC_KEYSYM.get(sym) == "onoff":
            self._power_key_held = False
            return "break"
        if sym in self.pressed_pc:
            self.pressed_pc.pop(sym, None)
            self.refresh_pressed()

    def on_focus_out(self, event):
        # Releasing Alt, Shift or directions in another window does not produce
        # KeyRelease events here. Prevent ghost keys when returning to the game.
        if event.widget is self.root:
            self.clear_keys()
            self.input_status.set("INPUT  •  focus lost (keys released)")

    def on_focus_in(self, event):
        if event.widget is self.root:
            self.root.after(60, self.refocus_input)

    def on_window_map(self, event):
        if event.widget is self.root:
            self.root.after(80, self.refocus_input)

    def refocus_input(self):
        """Give keyboard events to the calculator after a dialog or Alt-Tab.

        Use focus_set (not focus_force), to avoid grabbing focus away from
        other Windows applications when we're in the background.
        """
        if self._closing or not hasattr(self, "canvas"):
            return
        try:
            if self.root.focus_displayof() is not None:
                self.canvas.focus_set()
                self.input_status.set("INPUT  •  ready (calculator focused)")
        except tk.TclError:
            pass

    def clear_keys(self):
        self.pressed_pc.clear()
        self.mouse_pressed = None
        self._power_key_held = False
        self.refresh_pressed()

    def refresh_pressed(self):
        names = set(self.pressed_pc.values())
        if self.mouse_pressed is not None:
            names.add(self.mouse_pressed)
        with self.pressed_lock:
            self.pressed.clear()
            self.pressed.update(names)
        for k, widget in self.key_widgets.items():
            if k in names:
                widget.configure(bg=ACCENT, fg=BG)
            else:
                widget.configure(fg=FG, bg=("#325e68" if k in ("ok", "exe") else
                       "#36495c" if k in ("left", "right", "up", "down") else
                       "#3a4c5a" if k in ("home", "onoff", "shift", "alpha") else PANEL_2))

    def show_keymap(self):
        w = tk.Toplevel(self.root)
        w.title("PC keyboard shortcuts")
        w.configure(bg=BG)
        w.geometry("615x430")
        lines = [
            "ALL calculator keys are clickable on the main window.",
            "Hold mouse button for continuous keypress (e.g. duck).",
            "", "Arrows   = ← ↑ ↓ →        Enter / Space = OK       Numpad Enter = EXE",
            "Esc = Back    Home / F1 = Home     Backspace / Delete = ⌫",
            "Shift = Shift     Alt = Alpha        F8 = Power",
            "", "F2 Back     F3 Shift     F4 Alpha     F5 Toolbox",
            "F6 var        F7 x,n,t        F9 ans         F10 EXE",
            "F11 = Fullscreen in skinned UI (√ in plain UI)     F12 = x²", "",
            "Digits 0–9, numpad, + - * / , . ( ) ^ work as expected.",
            "", "x=x,n,t    v=var     b=Toolbox    e=eˣ    l=ln     g=log",
            "i=i       s=sin     c=cos      t=tan   p=π      q=√",
            "w=x²      r=^       n=×10ⁿ      a=ans", "",
            "Drag any .nwa into the window or click Open .nwa.",
            "Watch changed .nwa reloads it when a build finishes.",
            "Follow newest local build finds fresh .nwa files automatically.",
            "Emulated saves persist in ~/.numplay-emulator/saves/.",
        ]
        tk.Label(w, text="\n".join(lines), justify="left", bg=BG, fg=FG,
                 font=("Consolas", 10), padx=18, pady=14).pack(anchor="nw")

    def poll(self):
        if self._closing:
            return
        newest_frame = None
        last_status = None
        ended = None
        error = None
        while True:
            try:
                m = self.messages.get_nowait()
            except queue.Empty:
                break
            if m[0] == "frame":
                newest_frame = m
            elif m[0] == "status":
                last_status = m[1]
            elif m[0] == "ended":
                ended = m[1]
            elif m[0] == "error":
                error = m[1]
        if last_status and not self.powered_off:
            self.set_status(last_status)
        if not self.powered_off and newest_frame and not ended:
            _, raw, elapsed = newest_frame
            try:
                self._screen_rgb = rgb565_to_photo_bytes(raw)
                self.present_screen()
                self.time_label.set(f"{elapsed:,} ms (virtual)")
            except Exception as exc:
                self.set_status(f"Display error: {exc}")
        if ended and not self.powered_off:
            self.black_screen(ended)
        elif (self.worker is not None and self.worker.end_reason == "exited"
              and not self.powered_off):
            # Also handle a full message queue dropping an end notification.
            self.black_screen("Application exited — press Reload to restart")
        if error and not self.powered_off:
            self.set_status("Emulator error — see dialog")
            messagebox.showerror("NumPlay emulator", error)
        if self.pending is not None and (self.worker is None or not self.worker.is_alive()):
            self.start_pending()
        self._poll_id = self.root.after(UI_POLL_MS, self.poll)

    def watch(self):
        if self._closing:
            return
        try:
            if self.follow_var.get():
                latest = find_nwas()
                if latest and (self.current is None or latest[0] != self.current):
                    # Switch only when a new build is stable across checks.
                    self._maybe_reload(latest[0])
                elif self.current is not None and self.watch_var.get():
                    self._maybe_reload(self.current)
            elif self.current is not None and self.watch_var.get():
                self._maybe_reload(self.current)
        except OSError as exc:
            self.set_status(f"Auto-reload check: {exc}")
        self._watch_id = self.root.after(WATCH_MS, self.watch)

    def _maybe_reload(self, path: Path):
        stamp = fingerprint(path)
        if stamp is None:
            return
        if path == self.current and stamp == self.current_stamp:
            self.watch_candidate = None
            return
        marker = (str(path), stamp)
        if self.watch_candidate is None or self.watch_candidate[0] != marker:
            self.watch_candidate = (marker, 1)
            return
        self.watch_candidate = None
        self.load(path)

    def on_close(self):
        self._closing = True
        self.stop_event.set()
        self.clear_keys()
        # Worker is a daemon; allow a short non-blocking poll to save storage.
        self._close_deadline = time.monotonic() + 3.0
        self._finish_close()

    def _finish_close(self):
        if self.worker is None or not self.worker.is_alive() or time.monotonic() > self._close_deadline:
            for callback in (self._poll_id, self._watch_id):
                try:
                    self.root.after_cancel(callback)
                except tk.TclError:
                    pass
            self.root.destroy()
        else:
            self.root.after(100, self._finish_close)


# ---------- Calculator artwork + interface (merged from V4 skin) ----------
ART_SOURCE = "https://github.com/k-kuroguro/vscode-numworks/blob/master/resources/webview/background.png"
ART_LICENSE = "https://github.com/k-kuroguro/vscode-numworks/blob/master/resources/LICENSE"

# Matches resources/webview/index.css and index.ejs in the extension.
# Coordinates are normalized against the background.png image.
SCREEN = (0.1625, 0.085, 0.675, 0.263)
NAV = {
    "left": (0.020, 0.370, 0.150, 0.280),
    "right": (0.170, 0.370, 0.150, 0.280),
    "up": (0.120, 0.090, 0.100, 0.400),
    "down": (0.120, 0.530, 0.100, 0.400),
    "home": (0.410, 0.150, 0.165, 0.310),
    "onoff": (0.410, 0.554, 0.165, 0.310),
    "ok": (0.670, 0.310, 0.135, 0.380),
    "back": (0.833, 0.310, 0.135, 0.380),
}
# Extension's HTML ordering, grouped by the extension's CSS rows.
FUNCTIONS = (
    "shift", "alpha", "xnt", "var", "toolbox", "backspace",
    "exp", "ln", "log", "i", "comma", "power",
    "sin", "cos", "tan", "pi", "sqrt", "square",
)
DIGITS = (
    "7", "8", "9", "lparen", "rparen",
    "4", "5", "6", "mul", "div",
    "1", "2", "3", "plus", "minus",
    "0", "dot", "ee", "ans", "exe",
)


def skin_hotspots() -> dict[str, tuple[float, float, float, float]]:
    """Recover key rectangles from the extension's CSS, in image fractions."""
    kx, ky, kw, kh = 0.10, 0.385, 0.81, 0.565
    boxes = {}
    for key, (x, y, w, h) in NAV.items():
        # .nav itself is only 27% of the keyboard height.
        boxes[key] = (kx + x * kw, ky + y * kh * .27, w * kw, h * kh * .27)

    # CSS percentage top/bottom margins use the containing block WIDTH.
    for keys, top, left, width, per_row, cell_w, cell_h, mx, my in (
        (FUNCTIONS, .2675, .005, .98, 6, .1465, .10, .01, .017),
        (DIGITS, .565, .005, .98, 5, .16, .113, .02, .018),
    ):
        cw = kw * width       # child group width as a fraction of image width
        for index, key in enumerate(keys):
            row, col = divmod(index, per_row)
            x = kx + kw * left + cw * (col * (cell_w + 2 * mx) + mx)
            # Vertical CSS %-margins/paddings are percentages of group width
            # (pixel width), so convert image-width units into image-height units.
            # Exact source dimensions: background.png is 1160x2221.
            y = ky + kh * top + (cw * 1160 / 2221) * (row * (cell_h + 2 * my) + my)
            w = cw * cell_w
            h = (cw * 1160 / 2221) * cell_h
            boxes[key] = (x, y, w, h)
    return boxes


HOTSPOTS = skin_hotspots()
assert set(HOTSPOTS) == set(emu.KEYS), (set(emu.KEYS) - set(HOTSPOTS))
assert all(0 <= x < 1 and 0 <= y < 1 and x + w <= 1.01 and y + h <= 1.01
           for x, y, w, h in HOTSPOTS.values())


def installed_artwork() -> Path | None:
    """Use the user's own VS Code extension installation, without copying art."""
    locations = (
        Path.home() / ".vscode" / "extensions",
        Path.home() / ".vscode-insiders" / "extensions",
        Path.home() / ".vscode-oss" / "extensions",
    )
    files = []
    for root in locations:
        files.extend(root.glob("k-kuroguro.numworks-simulator-*/resources/webview/background.png"))
    files = [p for p in files if p.is_file()]
    return max(files, key=lambda x: x.stat().st_mtime_ns) if files else None


class CalculatorSkinApp(LiveApp):
    def __init__(self, root, initial, nwlink, model, system, auto_follow,
                 art_path: Path, max_sleep_ms: int = 0,
                 profile_speed: bool = False):
        self.art_path = art_path
        self.art = Image.open(art_path).convert("RGB")
        self.art_px = self.art.size
        self._canvas_image = None
        self._canvas_screen = None
        self._bg_photo = None
        self._art_geometry = (0, 0, 0, 0)
        self._hover = None
        self._last_canvas_size = None
        self.fullscreen = False
        self.f11_down = False
        self.rate_label = tk.StringVar(value="Measuring real-time speed…")
        super().__init__(root, initial, nwlink, model, system, auto_follow,
                         max_sleep_ms=max_sleep_ms, profile_speed=profile_speed)

    def _build_ui(self):
        self.root.title("NumPlay · NumWorks Live Emulator V9")
        self.root.geometry("1100x840")
        self.root.minsize(790, 520)
        # F11 has a conventional meaning in a desktop app. Square-root is
        # still available as an on-screen key or by pressing Q.

        body = tk.Frame(self.root, bg=BG)
        body.pack(fill="both", expand=True, padx=8, pady=8)
        sidebar = tk.Frame(body, bg=PANEL, width=312)
        sidebar.pack(side="right", fill="y", padx=(8, 0))
        sidebar.pack_propagate(False)
        left = tk.Frame(body, bg=BG)
        left.pack(side="left", fill="both", expand=True)
        self.canvas = tk.Canvas(left, bg=BG, bd=0, highlightthickness=0,
                                takefocus=1)
        self.canvas.pack(fill="both", expand=True)
        # Inherited LiveApp registers the screen label for drag and drop.
        self.screen_label = self.canvas
        self.canvas.bind("<Configure>", self._resize)
        self.canvas.bind("<ButtonPress-1>", self._mouse_press)
        self.canvas.bind("<Motion>", self._mouse_move)
        self.canvas.bind("<Leave>", self._mouse_leave)

        inner = tk.Frame(sidebar, bg=PANEL)
        inner.pack(fill="both", expand=True, padx=13, pady=(16, 10))
        tk.Label(inner, text="NUMWORKS  /  NUMPLAY", bg=PANEL,
                 fg=FG, font=("Segoe UI", 14, "bold"),
                 anchor="w").pack(fill="x")
        tk.Label(inner, text="Live calculator emulator", bg=PANEL,
                 fg=MUTED, font=("Segoe UI", 10),
                 anchor="w").pack(fill="x", pady=(2, 14))

        tk.Label(inner, text="LOADED APP", bg=PANEL, fg=ACCENT,
                 font=("Segoe UI", 9, "bold"), anchor="w").pack(fill="x")
        self.file_title = tk.Label(inner, text="No .nwa selected", bg=PANEL,
                                   fg=FG, font=("Segoe UI", 10, "bold"),
                                   anchor="w", wraplength=270, justify="left")
        self.file_title.pack(fill="x", pady=(3, 4))
        tk.Label(inner, textvariable=self.file_label, bg=PANEL,
                 fg=MUTED, font=("Segoe UI", 8), anchor="w",
                 wraplength=270, justify="left").pack(fill="x", pady=(0, 13))

        controls = tk.Frame(inner, bg=PANEL)
        controls.pack(fill="x")
        for col in (0, 1):
            controls.grid_columnconfigure(col, weight=1, uniform="actions")
        actions = (
            ("Open .nwa", self.open_file), ("Newest build", self.find_latest),
            ("Reload", self.reset), ("Pause", self.toggle_pause),
            ("Screenshot", self.screenshot), ("Keymap", self.show_keymap),
            ("Fullscreen  F11", self.toggle_fullscreen),
            ("Focus keyboard", self.refocus_input),
        )
        for idx, (label, command) in enumerate(actions):
            # GUI button focus is transient; restore calculator key focus once
            # dialogs (Open .nwa, Keymap) or actions finish.
            def command_then_focus(action=command):
                try:
                    action()
                finally:
                    self.root.after(60, self.refocus_input)
            button = tk.Button(controls, text=label, command=command_then_focus,
                               bg=PANEL_2, fg=FG, bd=0, relief="flat",
                               activebackground=ACCENT, activeforeground=BG,
                               font=("Segoe UI", 9), padx=4, pady=9)
            row, col = divmod(idx, 2)
            button.grid(row=row, column=col, sticky="ew", padx=3, pady=3,
                        columnspan=1)
            if label == "Pause":
                self.pause_button = button

        tk.Frame(inner, height=1, bg=PANEL_2).pack(fill="x", pady=(17, 13))
        for var, label in ((self.watch_var, "Auto-reload changed .nwa"),
                           (self.follow_var, "Follow newest local build")):
            tk.Checkbutton(inner, text=label, variable=var,
                           bg=PANEL, fg=FG,
                           selectcolor=PANEL_2,
                           activebackground=PANEL,
                           activeforeground=ACCENT,
                           font=("Segoe UI", 9), anchor="w").pack(fill="x", pady=3)

        tk.Frame(inner, height=1, bg=PANEL_2).pack(fill="x", pady=(14, 12))
        tk.Label(inner, text="EMULATED CPU CLOCK", bg=PANEL, fg=ACCENT,
                 font=("Segoe UI", 9, "bold"), anchor="w").pack(fill="x")
        self.auto_clock_var = tk.BooleanVar(value=True)
        tk.Checkbutton(inner, text="Automatic clock (recommended)",
                       variable=self.auto_clock_var, command=self.toggle_auto_clock,
                       bg=PANEL, fg=FG,
                       selectcolor=PANEL_2, activebackground=PANEL,
                       activeforeground=ACCENT,
                       font=("Segoe UI", 9), anchor="w").pack(fill="x", pady=(8, 2))
        self.clock_option = tk.StringVar(value="216 MHz  (default)")
        choices = ("216 MHz  (default)", "108 MHz", "54 MHz", "27 MHz")
        self.clock_menu = tk.OptionMenu(inner, self.clock_option, *choices, command=self.change_clock)
        self.clock_menu.configure(bg=PANEL_2, fg=FG,
                       activebackground=ACCENT, activeforeground=BG,
                       bd=0, highlightthickness=0, font=("Segoe UI", 10),
                       state="disabled")
        self.clock_menu["menu"].configure(bg=PANEL_2, fg=FG)
        self.clock_menu.pack(fill="x", pady=(6, 3))
        tk.Label(inner,
                 text="Auto adjusts gradually to CPU-heavy scenes; no frame "
                      "catch-up bursts. Disable Auto to set a manual clock. "
                      "Some games may still differ from real hardware.",
                 bg=PANEL, fg=MUTED, font=("Segoe UI", 9),
                 wraplength=270, justify="left", anchor="w").pack(fill="x", pady=(4, 9))
        tk.Label(inner, textvariable=self.rate_label, bg=PANEL, fg=ACCENT,
                 font=("Segoe UI", 10, "bold"), anchor="w").pack(fill="x")
        tk.Label(inner, textvariable=self.input_status, bg=PANEL, fg=MUTED,
                 font=("Segoe UI", 9), anchor="w").pack(fill="x", pady=(3, 0))
        tk.Label(inner, textvariable=self.time_label, bg=PANEL,
                 fg=MUTED, font=("Segoe UI", 9), anchor="w").pack(fill="x", pady=(3, 0))

        spacer = tk.Frame(inner, bg=PANEL)
        spacer.pack(fill="both", expand=True)
        self.drop_line = tk.Label(inner,
                                  text="Drop any .nwa onto the calculator\n"
                                       "Use its buttons or your PC keyboard",
                                  bg=PANEL, fg=MUTED,
                                  font=("Segoe UI", 9), justify="center",
                                  pady=9)
        self.drop_line.pack(fill="x")
        credits = tk.Label(inner,
                           text="Calculator skin: Upsilon / k-kuroguro\n"
                                "CC BY-NC-SA · View asset license ↗",
                           bg=PANEL, fg=MUTED, cursor="hand2",
                           font=("Segoe UI", 8), justify="center")
        credits.pack(fill="x", pady=(4, 7))
        credits.bind("<Button-1>", lambda e: webbrowser.open(ART_LICENSE))
        tk.Label(sidebar, textvariable=self.status, bg=PANEL_2,
                 fg=ACCENT, anchor="w", justify="left",
                 wraplength=282, padx=12, pady=10,
                 font=("Segoe UI", 9)).pack(fill="x", side="bottom")
        # Startup is user-initiated; focus once, as in the earlier working GUI.
        # On later Alt-Tab reactivation use non-stealing focus_set() instead.
        self.root.focus_force()
        self.root.after(150, self.refocus_input)

    def toggle_fullscreen(self):
        self.fullscreen = not self.fullscreen
        self.root.attributes("-fullscreen", self.fullscreen)
        # Force the image to refit after the window manager changes its size.
        self.root.after(50, self._resize)

    def on_pc_down(self, event):
        if event.keysym.lower() == "f11":
            if not self.f11_down:
                self.f11_down = True
                self.toggle_fullscreen()
            return "break"
        return super().on_pc_down(event)

    def on_pc_up(self, event):
        if event.keysym.lower() == "f11":
            self.f11_down = False
            return "break"
        return super().on_pc_up(event)

    def toggle_auto_clock(self):
        self.auto_clock[0] = bool(self.auto_clock_var.get())
        self.clock_menu.configure(state="disabled" if self.auto_clock[0] else "normal")
        if self.auto_clock[0]:
            self.set_status("Automatic CPU clock enabled; adjusts gradually.")
        else:
            self.clock_mhz[0] = int(self.clock_option.get().split()[0])
            self.set_status(f"Manual CPU clock: {self.clock_mhz[0]} MHz")

    def change_clock(self, selected):
        if self.auto_clock[0]:
            return
        self.clock_mhz[0] = int(selected.split()[0])
        self.rate_label.set("Measuring real-time speed…")
        self.set_status(f"Manual CPU clock set to {self.clock_mhz[0]} MHz")

    def present_screen(self):
        self._display_screen()

    def _resize(self, event=None):
        # Preserve calculator proportions and center it in the available space.
        ww = self.canvas.winfo_width()
        hh = self.canvas.winfo_height()
        if ww < 20 or hh < 20:
            return
        ratio = self.art_px[0] / self.art_px[1]
        art_w = max(1, int(min(ww - 8, (hh - 8) * ratio)))
        art_h = max(1, int(art_w / ratio))
        x0 = (ww - art_w) // 2
        y0 = (hh - art_h) // 2
        geom = (x0, y0, art_w, art_h)
        if geom == self._art_geometry:
            return
        self._art_geometry = geom
        self._bg_photo = ImageTk.PhotoImage(self.art.resize((art_w, art_h), Image.Resampling.LANCZOS))
        self.canvas.delete("all")
        self._canvas_image = self.canvas.create_image(x0, y0, image=self._bg_photo, anchor="nw", tags="art")
        self._display_screen()
        self._draw_highlight()

    def _rect(self, rect):
        x0, y0, aw, ah = self._art_geometry
        x, y, w, h = rect
        return (round(x0 + x * aw), round(y0 + y * ah),
                max(1, round(w * aw)), max(1, round(h * ah)))

    def _display_screen(self):
        if self._screen_rgb is None:
            return
        x, y, w, h = self._rect(SCREEN)
        photo = ImageTk.PhotoImage(self._screen_rgb.resize((w, h), Image.Resampling.NEAREST))
        self._photo = photo
        if self._canvas_screen is None or not self.canvas.find_withtag("screen"):
            self._canvas_screen = self.canvas.create_image(x, y, image=photo, anchor="nw", tags="screen")
        else:
            self.canvas.itemconfigure(self._canvas_screen, image=photo)
            self.canvas.coords(self._canvas_screen, x, y)

    def _key_at(self, x, y) -> str | None:
        if self._art_geometry[2] == 0:
            return None
        # A few nav keys' rectangular touch regions overlap: choose the closest
        # *center* of a containing key, consistent with the visible button art.
        matched = []
        for key, bounds in HOTSPOTS.items():
            bx, by, bw, bh = self._rect(bounds)
            if bx <= x <= bx + bw and by <= y <= by + bh:
                center_distance = ((x - bx - bw / 2) / bw) ** 2 + ((y - by - bh / 2) / bh) ** 2
                matched.append((center_distance, key))
        return min(matched)[1] if matched else None

    def _draw_highlight(self):
        self.canvas.delete("key-light")
        key = self.mouse_pressed or self._hover
        if not key:
            return
        x, y, w, h = self._rect(HOTSPOTS[key])
        held = key == self.mouse_pressed or key in self.pressed_pc.values()
        self.canvas.create_oval(x + 2, y + 2, x + w - 2, y + h - 2,
                                outline=ACCENT if held else "#ffffff",
                                width=2, tags="key-light")

    def _mouse_press(self, event):
        # Crucial on Windows: clicking the calculator after using a sidebar
        # button must direct the NEXT real keyboard press to the calculator.
        self.canvas.focus_set()
        key = self._key_at(event.x, event.y)
        if key:
            self.on_mouse_down(key)
            return "break"
        return None

    def _mouse_move(self, event):
        self._hover = self._key_at(event.x, event.y)
        self.canvas.configure(cursor="hand2" if self._hover else "arrow")
        self._draw_highlight()

    def _mouse_leave(self, event):
        self._hover = None
        self._draw_highlight()

    def refresh_pressed(self):
        names = set(self.pressed_pc.values())
        if self.mouse_pressed is not None:
            names.add(self.mouse_pressed)
        with self.pressed_lock:
            self.pressed.clear()
            self.pressed.update(names)
        if hasattr(self, "canvas"):
            self._draw_highlight()

    def poll(self):
        if self._closing:
            return
        newest_frame = None
        last_status = None
        latest_pace = None
        ended = None
        error = None
        while True:
            try:
                m = self.messages.get_nowait()
            except queue.Empty:
                break
            if m[0] == "frame":
                newest_frame = m
            elif m[0] == "status":
                last_status = m[1]
            elif m[0] == "pace":
                latest_pace = m
            elif m[0] == "ended":
                ended = m[1]
            elif m[0] == "error":
                error = m[1]
        if last_status and not self.powered_off:
            self.set_status(last_status)
        if latest_pace and not self.powered_off:
            _, factor, mhz, auto = latest_pace
            self.rate_label.set(f"Speed: {factor:.2f}×  •  CPU: {mhz} MHz" +
                                (" (auto)" if auto else " (manual)"))
        if self.current:
            self.file_title.configure(text=self.current.name)
        if (self.worker is not None and self.worker.is_alive() and
            not self.powered_off and not self.pause_event.is_set() and
            time.monotonic() - self._last_frame_wall > 3.0):
            phase = self.worker.phase
            age = time.monotonic() - self.worker.phase_since_wall
            if phase == "running ARM CPU":
                self.rate_label.set(f"Slow ARM emulation: {age:.1f}s in CPU step")
            elif phase == "waiting for app sleep":
                detail = self.worker.last_svc_detail
                if detail:
                    svc, where, duration = detail
                    self.rate_label.set(f"Pacing {duration:.0f}ms SVC {svc} at 0x{where:x}")
                else:
                    self.rate_label.set("Waiting for emulated app timing")
            elif phase == "waiting for emulated time":
                self.rate_label.set("Pacing virtual clock (not an ARM crash)")
            else:
                self.rate_label.set(f"Worker: {phase} ({age:.1f}s)")
            # If a real .nwa still wedges, this tells us *where*, without
            # printing private save data or flooding PowerShell each frame.
            now = time.monotonic()
            if now - self._last_stall_log_wall > 5.0:
                self._last_stall_log_wall = now
                print(f"[NumPlay emulator diagnostic] no frame for "
                      f"{now-self._last_frame_wall:.1f}s; phase={phase}; "
                      f"phase_age={age:.1f}s; virtual_ms={self.worker.last_progress_ms:.0f}; "
                      f"last_cpu_step={self.worker.last_cpu_seconds:.3f}s; "
                      f"last_virtual_step={self.worker.last_virtual_delta:.1f}ms; "
                      f"app_sleep_step={self.worker.last_sleep_delta_ms:.1f}ms; "
                      f"last_svc={self.worker.last_svc_detail}; "
                      f"CPU={self.clock_mhz[0]}MHz; auto={self.auto_clock[0]}",
                      file=sys.stderr, flush=True)
        if newest_frame and not self.powered_off and not ended:
            self._last_frame_wall = time.monotonic()
            _, raw, elapsed = newest_frame
            try:
                self._screen_rgb = rgb565_to_photo_bytes(raw)
                self.present_screen()
                self.time_label.set(f"{elapsed:,} ms (virtual)")
            except Exception as exc:
                self.set_status(f"Display error: {exc}")
        if ended and not self.powered_off:
            self.black_screen(ended)
        elif (self.worker is not None and self.worker.end_reason == "exited"
              and not self.powered_off):
            self.black_screen("Application exited — press Power or Reload to restart")
        if error and not self.powered_off:
            self.set_status("Emulator error — see dialog")
            messagebox.showerror("NumPlay emulator", error)
        if self.pending is not None and (self.worker is None or not self.worker.is_alive()):
            self.start_pending()
        self._poll_id = self.root.after(UI_POLL_MS, self.poll)


def main():
    ap = argparse.ArgumentParser(description="NumWorks-shaped live .nwa emulator for Windows")
    ap.add_argument("nwa", nargs="?", type=Path, help="optional .nwa path, or drop it after launching")
    ap.add_argument("--nwlink", default=nwlink_command())
    ap.add_argument("--skin-image", type=Path, help="path to extension's resources/webview/background.png")
    ap.add_argument("--model", choices=("n0120", "n0110"), default="n0120")
    ap.add_argument("--system", choices=("epsilon", "upsilon"), default="epsilon")
    ap.add_argument("--follow-newest", action="store_true")
    ap.add_argument("--profile-speed", action="store_true",
                    help="print a measured ARM/render/pacing speed report every 5 seconds")
    ap.add_argument("--max-sleep-ms", type=int, default=0, metavar="MS",
                    help="EXPERIMENTAL: cap any EADK app sleep to MS milliseconds; "
                         "0 (default) preserves exact emulator behavior")
    args = ap.parse_args()
    if args.max_sleep_ms < 0 or args.max_sleep_ms > 10000:
        ap.error("--max-sleep-ms must be between 0 and 10000")
    if args.max_sleep_ms:
        print(f"[NumPlay diagnostic] EXPERIMENTAL sleep guard ON: "
              f"sleep calls over {args.max_sleep_ms}ms will be shortened. "
              "Game timing may differ from the calculator.", file=sys.stderr)
    if not args.nwlink:
        ap.error("nwlink not found. Run: npm install -g nwlink@1.0.0")
    art = args.skin_image or installed_artwork()
    if not art or not art.is_file():
        ap.error("NumWorks skin not found. Install k-kuroguro's Numworks Simulator in VS Code, "
                 "or pass --skin-image PATH pointing to resources/webview/background.png.\n"
                 f"Image source: {ART_SOURCE}")
    if TkinterDnD:
        root = TkinterDnD.Tk()
    else:
        root = tk.Tk()
        print("Tip: pip install tkinterdnd2 for .nwa drag-and-drop.")
    CalculatorSkinApp(root, args.nwa, args.nwlink, args.model, args.system,
                      args.follow_newest, art, max_sleep_ms=args.max_sleep_ms,
                      profile_speed=args.profile_speed)
    root.mainloop()


if __name__ == "__main__":
    main()
