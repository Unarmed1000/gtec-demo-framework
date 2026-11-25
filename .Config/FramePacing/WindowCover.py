#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery
# * All rights reserved.
# *
# * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
# *
# * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
# * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
# *    documentation and/or other materials provided with the distribution.
# * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
# *    software without specific prior written permission.
# *
# * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
# * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
# * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# ****************************************************************************************************************************************************
"""Covers the window of an app that the capture tool started, for a while, and shows it again.

A run that measures what a pacer does while its window is not shown (a window that is covered presents without being held by the display,
and a pacer that relies on that hold has to see it) needs the window covered at a known time and shown again at another. This puts an
opaque window without a frame over the window of the process, above every other window, from one time to another, and writes when it did
to the file it is given, as seconds since it started and as wall clock times, so a run's notes can say when the window was covered.

With the mode minimize the window is minimized instead, and restored again (without taking the keyboard focus).

Run as its own process (it has a message loop of its own):
  WindowCover.py <process id> <cover after seconds> <show again after seconds> <times file> [cover|minimize]

Windows only. Elsewhere it writes that nothing was done and returns.
"""

import ctypes
import datetime
import sys
import time

_g_findTimeoutSeconds = 15.0
_g_pollSeconds = 0.05


class _RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long), ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


def _FindWindow(processId: int) -> tuple[int, tuple[int, int, int, int]] | None:
    """The handle and the screen rectangle of the largest visible top level window of the process, or None"""
    user32 = ctypes.windll.user32
    found: list[tuple[int, tuple[int, int, int, int]]] = []
    enumProc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def OnWindow(hWnd: int, _lParam: int) -> bool:
        windowProcessId = ctypes.c_ulong(0)
        user32.GetWindowThreadProcessId(ctypes.c_void_p(hWnd), ctypes.byref(windowProcessId))
        if windowProcessId.value == processId and user32.IsWindowVisible(ctypes.c_void_p(hWnd)):
            rect = _RECT()
            if user32.GetWindowRect(ctypes.c_void_p(hWnd), ctypes.byref(rect)):
                found.append((hWnd, (rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top)))
        return True

    user32.EnumWindows(enumProc(OnWindow), None)
    if len(found) == 0:
        return None
    return max(found, key=lambda entry: entry[1][2] * entry[1][3])


def _FindWindowRect(processId: int) -> tuple[int, int, int, int] | None:
    """The screen rectangle of the largest visible top level window of the process, or None"""
    window = _FindWindow(processId)
    return window[1] if window is not None else None


def _Now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="milliseconds")


def Main(argv: list[str]) -> int:
    if len(argv) not in (5, 6):
        print(__doc__)
        return 2
    mode = argv[5] if len(argv) == 6 else "cover"
    if mode not in ("cover", "minimize"):
        print(__doc__)
        return 2
    processId = int(argv[1])
    coverAfterSeconds = float(argv[2])
    showAfterSeconds = float(argv[3])
    timesPath = argv[4]
    startTime = time.monotonic()
    lines: list[str] = []

    def Write() -> None:
        with open(timesPath, "w", encoding="utf-8") as file:
            file.write("\n".join(lines) + "\n")

    if sys.platform != "win32":
        lines.append("cover=not supported on this platform")
        Write()
        return 0

    # The window is looked for until it shows up, then covered at its time
    rect = None
    while rect is None and time.monotonic() - startTime < max(_g_findTimeoutSeconds, coverAfterSeconds):
        rect = _FindWindowRect(processId)
        if rect is None:
            time.sleep(_g_pollSeconds)
    if rect is None:
        lines.append("cover=no window of the process was found")
        Write()
        return 1
    while time.monotonic() - startTime < coverAfterSeconds:
        time.sleep(_g_pollSeconds)
    if mode == "minimize":
        window = _FindWindow(processId)
        if window is None:
            lines.append("cover=the window of the process was gone")
            Write()
            return 1
        user32 = ctypes.windll.user32
        # SW_SHOWMINNOACTIVE, and SW_SHOWNOACTIVATE to restore it: the keyboard focus stays where it is
        user32.ShowWindow(ctypes.c_void_p(window[0]), 7)
        lines.append("mode=minimize")
        lines.append(f"coveredAtSeconds={time.monotonic() - startTime:.3f}")
        lines.append(f"coveredAt={_Now()}")
        while time.monotonic() - startTime < showAfterSeconds:
            time.sleep(_g_pollSeconds)
        user32.ShowWindow(ctypes.c_void_p(window[0]), 4)
        lines.append(f"shownAgainAtSeconds={time.monotonic() - startTime:.3f}")
        lines.append(f"shownAgainAt={_Now()}")
        Write()
        return 0

    # Where the window is now (it can have moved since it was found)
    rect = _FindWindowRect(processId) or rect
    lines.append("mode=cover")

    import tkinter

    root = tkinter.Tk()
    root.overrideredirect(True)
    root.attributes("-topmost", True)
    root.configure(background="black")
    # A little larger than the window, so no edge of it shows
    margin = 8
    root.geometry(f"{rect[2] + 2 * margin}x{rect[3] + 2 * margin}+{rect[0] - margin}+{rect[1] - margin}")
    root.update()
    lines.append(f"coveredAtSeconds={time.monotonic() - startTime:.3f}")
    lines.append(f"coveredAt={_Now()}")
    lines.append(f"coveredRect={rect[0]},{rect[1]},{rect[2]},{rect[3]}")

    def Uncover() -> None:
        lines.append(f"shownAgainAtSeconds={time.monotonic() - startTime:.3f}")
        lines.append(f"shownAgainAt={_Now()}")
        root.destroy()

    root.after(max(int((showAfterSeconds - (time.monotonic() - startTime)) * 1000.0), 0), Uncover)
    root.mainloop()
    Write()
    return 0


if __name__ == "__main__":
    sys.exit(Main(sys.argv))
