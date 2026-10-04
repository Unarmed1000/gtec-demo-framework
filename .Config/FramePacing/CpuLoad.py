#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery ApS
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

# A CPU load for the frame pacing captures: a number of processes that are busy a chosen share of the time. It stands in for the other work a
# machine does while an app runs (a build, for example), so a capture can be repeated with the same load.
#
# It runs until it is stopped: Ctrl+C, the end of --duration, or with --stop-on-stdin the end of its standard input (how FramePacingCapture.py
# stops it, a process that is killed takes its load with it as the workers watch their parent).

import argparse
import multiprocessing
import multiprocessing.synchronize
import os
import sys
import threading
import time

_g_defaultPeriodMs = 10.0
_g_workerJoinTimeoutSeconds = 5.0


def _RunWorker(stopEvent: multiprocessing.synchronize.Event, duty: float, periodSeconds: float) -> None:
    """Busy for 'duty' of every period until asked to stop, or until the process that started it is gone"""
    parent = multiprocessing.parent_process()
    busySeconds = periodSeconds * duty
    try:
        while not stopEvent.is_set() and (parent is None or parent.is_alive()):
            periodStart = time.perf_counter()
            busyEnd = periodStart + busySeconds
            while time.perf_counter() < busyEnd:
                pass
            remainingSeconds = (periodStart + periodSeconds) - time.perf_counter()
            if remainingSeconds > 0.0:
                time.sleep(remainingSeconds)
    except KeyboardInterrupt:
        pass


def _WaitForStdinToEnd(stopEvent: multiprocessing.synchronize.Event) -> None:
    try:
        while sys.stdin.readline() != "":
            pass
    except (OSError, ValueError):
        pass
    stopEvent.set()


def ResolveProcessCount(processes: int) -> int:
    """Zero means one process for every logical CPU"""
    if processes > 0:
        return processes
    cpuCount = os.cpu_count()
    return cpuCount if cpuCount is not None else 1


def _CreateParser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="A CPU load: processes that are busy a share of the time, until stopped.")
    parser.add_argument("--processes", type=int, default=0, help="The number of busy processes (0 = one for every logical CPU, the default).")
    parser.add_argument("--duty", type=float, default=1.0, help="The share of the time a process is busy, 0.05 to 1.0 (default 1.0).")
    parser.add_argument("--period-ms", type=float, default=_g_defaultPeriodMs,
                        help=f"The length of one busy and idle cycle in milliseconds (default {_g_defaultPeriodMs}).")
    parser.add_argument("--duration", type=float, default=0.0, help="Stop after this many seconds (0 = run until stopped, the default).")
    parser.add_argument("--stop-on-stdin", action="store_true", help="Stop when the standard input ends (used by FramePacingCapture.py).")
    return parser


def Main(argv: list[str]) -> int:
    args = _CreateParser().parse_args(argv)
    if args.processes < 0:
        print("ERROR: --processes can not be negative", file=sys.stderr)
        return 2
    if not 0.05 <= args.duty <= 1.0:
        print("ERROR: --duty must be 0.05 to 1.0", file=sys.stderr)
        return 2
    if args.period_ms <= 0.0 or args.duration < 0.0:
        print("ERROR: --period-ms must be above zero and --duration can not be negative", file=sys.stderr)
        return 2

    processCount = ResolveProcessCount(args.processes)
    stopEvent = multiprocessing.Event()
    workers = [multiprocessing.Process(target=_RunWorker, args=(stopEvent, args.duty, args.period_ms / 1000.0), daemon=True)
               for _ in range(processCount)]
    for worker in workers:
        worker.start()
    print(f"CpuLoad: {processCount} processes, busy {args.duty * 100.0:.0f} % of every {args.period_ms:g} ms", flush=True)

    if args.stop_on_stdin:
        threading.Thread(target=_WaitForStdinToEnd, args=(stopEvent,), daemon=True).start()
    try:
        stopEvent.wait(args.duration if args.duration > 0.0 else None)
    except KeyboardInterrupt:
        pass
    stopEvent.set()
    for worker in workers:
        worker.join(_g_workerJoinTimeoutSeconds)
        if worker.is_alive():
            worker.terminate()
    print("CpuLoad: stopped", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
