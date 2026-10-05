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
"""Keeps the window of an app that the capture tool started on top of the other windows.

A window that is covered by another one is not shown, and the presents of an app whose window is not shown say nothing about frame
pacing. Windows does not let a program that was started from a background process take the foreground while the person at the
machine works in another window, so the window of a captured app can open behind the editor or the terminal the capture was
started from. A window can be made topmost by any program though: it is then above the other windows without taking the keyboard
focus, which also keeps key presses that are meant for something else away from the app.

Windows only. Elsewhere nothing is done and the caller is told so.
"""

import ctypes
import sys
import threading
import time

_g_pollSeconds = 0.05


class WindowOnTop:
    """Looks for the top level windows of a process on a thread and makes them topmost when they show up."""

    def __init__(self, processId: int, timeoutSeconds: float = 15.0) -> None:
        self.ProcessId = processId
        self.TimeoutSeconds = timeoutSeconds
        # True once a window of the process was made topmost
        self.Done = False
        self.__stop = threading.Event()
        self.__thread: threading.Thread | None = None
        if sys.platform == "win32":
            self.__thread = threading.Thread(target=self.__Run, daemon=True)
            self.__thread.start()

    @staticmethod
    def IsSupported() -> bool:
        return sys.platform == "win32"

    def Stop(self) -> None:
        self.__stop.set()
        if self.__thread is not None:
            self.__thread.join(1.0)

    def __Run(self) -> None:
        endTime = time.monotonic() + self.TimeoutSeconds
        while not self.__stop.is_set() and time.monotonic() < endTime:
            if _TryMakeTopmost(self.ProcessId):
                self.Done = True
                return
            time.sleep(_g_pollSeconds)


def _TryMakeTopmost(processId: int) -> bool:
    user32 = ctypes.windll.user32
    found: list[int] = []
    enumProc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def OnWindow(hWnd: int, _lParam: int) -> bool:
        windowProcessId = ctypes.c_ulong(0)
        user32.GetWindowThreadProcessId(ctypes.c_void_p(hWnd), ctypes.byref(windowProcessId))
        if windowProcessId.value == processId and user32.IsWindowVisible(ctypes.c_void_p(hWnd)):
            found.append(hWnd)
        return True

    user32.EnumWindows(enumProc(OnWindow), None)
    hWndTopmost = ctypes.c_void_p(-1)
    # SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW: above the other windows where it is, without the keyboard focus
    flags = 0x0001 | 0x0002 | 0x0010 | 0x0040
    result = False
    for hWnd in found:
        user32.SetWindowPos.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint]
        if user32.SetWindowPos(ctypes.c_void_p(hWnd), hWndTopmost, 0, 0, 0, 0, flags):
            result = True
    return result
