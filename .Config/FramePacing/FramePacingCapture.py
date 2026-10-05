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

# Captures frame pacing logs: runs an app with '--FramePacing.Log' once for every run of a plan, one run at a time, and writes the notes of the
# run next to its log. A run a plan marks as 'loaded' is run twice, first on the idle machine and then with a CPU load from CpuLoad.py.
#
#   FramePacingCapture.py list                      the plans that come with the tool
#   FramePacingCapture.py run <plan> -o <dir>       capture the runs of a plan
#   FramePacingCapture.py check <log.csv>...        the notes of logs that exist already
#   FramePacingCapture.py calibrate                 find the GPU load settings that give a share of a refresh
#
# How to prepare the machine and how to read the files is described in Doc/FramePacingCapture.md.

import argparse
import ctypes
import datetime
import os
import platform
import subprocess
import sys
import time
import tomllib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import CpuLoad
from FramePacingAnonymise import AnonymiseFiles
from FramePacingAnonymise import GetLocalPaths
from FramePacingLogFile import FramePacingLogFile, ToEventsPath, g_ticksPerMillisecond, g_ticksPerSecond
from FramePacingRunCheck import CheckRun, FormatReport, FormatSummary, RunCheck, RunExpectation
from WindowOnTop import WindowOnTop

_g_toolDirectory = Path(__file__).resolve().parent
_g_planDirectory = _g_toolDirectory / "Plans"
_g_sdkEnvironmentVariable = "FSL_GRAPHICS_SDK"
_g_defaultApp = "Vulkan.FramePacing"
_g_defaultFrames = 2000
_g_defaultPauseSeconds = 3.0
_g_defaultTimeoutSeconds = 300.0
# How long the load runs before the app is started, so the machine is as busy as it gets
_g_loadSettleSeconds = 2.0
_g_loadStopTimeoutSeconds = 10.0
_g_queryTimeoutSeconds = 10.0
_g_idleSuffix = "_idle"
_g_loadedSuffix = "_loaded"
_g_nvidiaVendorId = 0x10de
_g_defaultCalibrateSteps = "0,16,64,128,256,384,512,768,1024"
_g_defaultCalibratePercent = "20,90,130"
_g_defaultCalibrateWindow = "[0,0,1600,900]"


class CaptureError(Exception):
    pass


@dataclass(frozen=True)
class LoadConfig:
    Processes: int
    Duty: float

    def Describe(self) -> str:
        return f"CpuLoad.py with {CpuLoad.ResolveProcessCount(self.Processes)} processes that are busy {self.Duty * 100.0:.0f} % of the time"


@dataclass(frozen=True)
class RunConfig:
    Name: str
    What: str
    Arguments: list[str]
    Frames: int
    Load: LoadConfig | None
    # None: the run is not one of an idle and loaded pair
    Loaded: bool | None
    RefreshRateHz: float | None


@dataclass
class Plan:
    SourcePath: Path
    Description: str
    App: str
    PauseSeconds: float
    TimeoutSeconds: float
    Variables: dict[str, str]
    Runs: list[RunConfig] = field(default_factory=list)


# ----------------------------------------------------------------------------------------------------------------------------------------------------
# The plan
# ----------------------------------------------------------------------------------------------------------------------------------------------------


def ResolvePlanPath(plan: str) -> Path:
    """A plan is the path of a file or the name of one of the plans that come with the tool"""
    path = Path(plan)
    if path.is_file():
        return path
    for candidate in (_g_planDirectory / plan, _g_planDirectory / (plan + ".toml")):
        if candidate.is_file():
            return candidate
    raise CaptureError(f"The plan '{plan}' is not a file and not one of the plans in '{_g_planDirectory}'")


def _GetStringList(table: dict[str, Any], key: str, where: str) -> list[str]:
    value = table.get(key, [])
    if not isinstance(value, list) or not all(isinstance(entry, str) for entry in value):
        raise CaptureError(f"{where}: '{key}' must be a list of strings (write numbers as strings: \"16\")")
    return value


def _ApplyVariables(texts: list[str], variables: dict[str, str], where: str) -> list[str]:
    result: list[str] = []
    for text in texts:
        for key, value in variables.items():
            text = text.replace("{" + key + "}", value)
        if "{" in text and "}" in text:
            raise CaptureError(f"{where}: '{text}' uses a variable the plan does not define")
        result.append(text)
    return result


@dataclass
class PlanOverrides:
    """What the command line sets instead of the plan, None where the plan decides"""
    Variables: dict[str, str] = field(default_factory=dict)
    Frames: int | None = None
    RefreshRateHz: float | None = None
    LoadProcesses: int | None = None
    LoadDuty: float | None = None
    # The machine is under a load the person started: a loaded run is run once, without its idle twin and without CpuLoad.py
    ExternalLoad: bool = False


def LoadPlan(path: Path, overrides: PlanOverrides) -> Plan:
    with open(path, "rb") as file:
        try:
            content = tomllib.load(file)
        except tomllib.TOMLDecodeError as ex:
            raise CaptureError(f"{path}: {ex}") from ex

    variables = {str(key): str(value) for key, value in content.get("variables", {}).items()}
    for key, value in overrides.Variables.items():
        if key not in variables:
            raise CaptureError(f"{path}: --set {key}: the plan has no variable '{key}' (it has: {', '.join(variables) or 'none'})")
        variables[key] = value

    loadTable = content.get("load", {})
    load = LoadConfig(overrides.LoadProcesses if overrides.LoadProcesses is not None else int(loadTable.get("processes", 0)),
                      overrides.LoadDuty if overrides.LoadDuty is not None else float(loadTable.get("duty", 1.0)))
    if load.Processes < 0 or not 0.05 <= load.Duty <= 1.0:
        raise CaptureError(f"{path}: [load] needs processes of zero or more and a duty of 0.05 to 1.0")

    plan = Plan(path, str(content.get("description", "")), str(content.get("app", _g_defaultApp)),
                float(content.get("pause_seconds", _g_defaultPauseSeconds)), float(content.get("timeout_seconds", _g_defaultTimeoutSeconds)), variables)
    planFrames = int(content.get("frames", _g_defaultFrames))
    planRefreshRateHz = overrides.RefreshRateHz if overrides.RefreshRateHz is not None else content.get("expected_refresh_hz")
    commonArguments = _GetStringList(content, "args", str(path))

    names: set[str] = set()
    for index, runTable in enumerate(content.get("run", [])):
        where = f"{path}: run {index + 1}"
        name = runTable.get("name")
        if not isinstance(name, str) or name == "" or any(character in name for character in "\\/:*?\"<>| "):
            raise CaptureError(f"{where}: needs a 'name' that can be used as a file name")
        if name in names:
            raise CaptureError(f"{where}: the name '{name}' is used twice")
        names.add(name)
        arguments = _ApplyVariables(commonArguments + _GetStringList(runTable, "args", where), variables, where)
        frames = overrides.Frames if overrides.Frames is not None else int(runTable.get("frames", planFrames))
        if frames < 1:
            raise CaptureError(f"{where}: 'frames' must be one or more")
        refreshRateHz = planRefreshRateHz if overrides.RefreshRateHz is not None else runTable.get("expected_refresh_hz", planRefreshRateHz)
        refreshRateHz = float(refreshRateHz) if refreshRateHz is not None else None
        what = _ApplyVariables([str(runTable.get("what", ""))], variables, where)[0]
        if bool(runTable.get("loaded", False)) and overrides.ExternalLoad:
            plan.Runs.append(RunConfig(name, what + " (machine under an external load)", arguments, frames, None, True, refreshRateHz))
        elif bool(runTable.get("loaded", False)):
            # The idle twin is always taken right before the loaded run, so the two differ in the load only
            plan.Runs.append(RunConfig(name + _g_idleSuffix, what + " (idle machine)", arguments, frames, None, False, refreshRateHz))
            plan.Runs.append(RunConfig(name + _g_loadedSuffix, what + " (machine under load)", arguments, frames, load, True, refreshRateHz))
        else:
            plan.Runs.append(RunConfig(name, what, arguments, frames, None, None, refreshRateHz))
    if len(plan.Runs) == 0:
        raise CaptureError(f"{path}: the plan has no runs")
    return plan


# ----------------------------------------------------------------------------------------------------------------------------------------------------
# The machine
# ----------------------------------------------------------------------------------------------------------------------------------------------------


class _DEVMODEW(ctypes.Structure):
    _fields_ = [("dmDeviceName", ctypes.c_wchar * 32), ("dmSpecVersion", ctypes.c_ushort), ("dmDriverVersion", ctypes.c_ushort),
                ("dmSize", ctypes.c_ushort), ("dmDriverExtra", ctypes.c_ushort), ("dmFields", ctypes.c_ulong), ("dmPositionX", ctypes.c_long),
                ("dmPositionY", ctypes.c_long), ("dmDisplayOrientation", ctypes.c_ulong), ("dmDisplayFixedOutput", ctypes.c_ulong),
                ("dmColor", ctypes.c_short), ("dmDuplex", ctypes.c_short), ("dmYResolution", ctypes.c_short), ("dmTTOption", ctypes.c_short),
                ("dmCollate", ctypes.c_short), ("dmFormName", ctypes.c_wchar * 32), ("dmLogPixels", ctypes.c_ushort),
                ("dmBitsPerPel", ctypes.c_ulong), ("dmPelsWidth", ctypes.c_ulong), ("dmPelsHeight", ctypes.c_ulong),
                ("dmDisplayFlags", ctypes.c_ulong), ("dmDisplayFrequency", ctypes.c_ulong), ("dmICMMethod", ctypes.c_ulong),
                ("dmICMIntent", ctypes.c_ulong), ("dmMediaType", ctypes.c_ulong), ("dmDitherType", ctypes.c_ulong), ("dmReserved1", ctypes.c_ulong),
                ("dmReserved2", ctypes.c_ulong), ("dmPanningWidth", ctypes.c_ulong), ("dmPanningHeight", ctypes.c_ulong)]


def _TryDescribePrimaryDisplayMode() -> str | None:
    """The mode of the primary display as Windows reports it (it only knows the refresh rate as a whole number)"""
    if platform.system() != "Windows":
        return None
    try:
        mode = _DEVMODEW()
        mode.dmSize = ctypes.sizeof(_DEVMODEW)
        enumCurrentSettings = 0xFFFFFFFF
        if ctypes.windll.user32.EnumDisplaySettingsW(None, enumCurrentSettings, ctypes.byref(mode)) == 0:
            return None
        return f"{mode.dmPelsWidth}x{mode.dmPelsHeight}, {mode.dmBitsPerPel} bits, {mode.dmDisplayFrequency} Hz"
    except (AttributeError, OSError):
        return None


def _TryDescribePowerPlan() -> str | None:
    """How the system trades speed for power: it decides how fast a CPU that was idle is running again, so it changes the timing of a frame"""
    try:
        if platform.system() == "Windows":
            result = subprocess.run(["powercfg", "/getactivescheme"], capture_output=True, text=True, timeout=_g_queryTimeoutSeconds, check=False)
            # 'Power Scheme GUID: 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c  (High performance)'
            _, separator, scheme = result.stdout.strip().partition(":")
            return " ".join(scheme.split()) if result.returncode == 0 and separator != "" else None
        governorPath = Path("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor")
        return f"cpufreq governor {governorPath.read_text(encoding='utf-8').strip()}" if governorPath.is_file() else None
    except (OSError, subprocess.SubprocessError):
        return None


def _DescribeCpu() -> str:
    try:
        if platform.system() == "Windows":
            import winreg
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
                return str(winreg.QueryValueEx(key, "ProcessorNameString")[0]).strip()
        with open("/proc/cpuinfo", encoding="utf-8") as file:
            for line in file:
                if line.startswith("model name"):
                    return line.partition(":")[2].strip()
    except OSError:
        pass
    return platform.processor() or platform.machine()


def _DescribeMachine(hardwareNames: bool) -> list[str]:
    # The model of the CPU is only named when it is asked for (--hardware-names)
    cpu = f"{_DescribeCpu()}, {os.cpu_count()} logical CPUs" if hardwareNames else f"{os.cpu_count()} logical CPUs"
    lines = [f"os: {platform.platform()}", f"cpu: {cpu}"]
    powerPlan = _TryDescribePowerPlan()
    lines.append(f"power plan: {powerPlan if powerPlan is not None else 'not known'}")
    displayMode = _TryDescribePrimaryDisplayMode()
    if displayMode is not None:
        lines.append(f"primary display mode (from the system, whole Hz): {displayMode}")
    return lines


def _DescribeGraphics(log: FramePacingLogFile) -> list[str]:
    """What the app found, from the facts of its log"""
    facts = log.Facts
    lines: list[str] = []
    if "api" in facts:
        lines.append(f"graphics api: {facts['api']} (version {facts.get('apiVersion', '?')})")
    if "vulkan.deviceName" in facts:
        driver = facts.get("vulkan.driverVersion", "?")
        if driver.isdigit() and int(facts.get("vulkan.vendorId", "0"), 16) == _g_nvidiaVendorId:
            # NVIDIA packs its driver version as 10.8.8.6 bits
            raw = int(driver)
            driver = f"{raw >> 22}.{(raw >> 14) & 0xff:02} (raw {raw})"
        lines.append(f"gpu: {facts['vulkan.deviceName']}, driver {driver}, Vulkan {facts.get('vulkan.apiVersion', '?')}")
    for event in log.GetEvents("window")[:1]:
        values = event.GetValues()
        lines.append(f"window: {values.get('widthPx', '?')}x{values.get('heightPx', '?')} pixels")
    for event in log.GetEvents("swapchainCreated")[:1]:
        lines.append(f"swapchain: {event.Details}")
    for event in log.GetEvents("presentTiming")[:1]:
        lines.append(f"present timing: {event.Details}")
    for event in log.GetEvents("pacerConfig"):
        lines.append(f"pacer settings at frame {event.FrameIndex}: {event.Details}")
    return lines


# ----------------------------------------------------------------------------------------------------------------------------------------------------
# Running
# ----------------------------------------------------------------------------------------------------------------------------------------------------


def FindExecutable(app: str, exeArgument: str | None) -> Path:
    if exeArgument is not None:
        path = Path(exeArgument)
        if not path.is_file():
            raise CaptureError(f"--exe: '{path}' is not a file")
        return path.resolve()
    sdkPath = os.environ.get(_g_sdkEnvironmentVariable)
    if sdkPath is None:
        raise CaptureError(f"The environment variable {_g_sdkEnvironmentVariable} is not set, so the app can not be found: use --exe")
    buildPath = Path(sdkPath) / "build"
    fileNames = {app, app + ".exe"}
    candidates = [path for path in buildPath.rglob(app + "*") if path.name in fileNames and path.is_file() and path.parent.name == "Release"]
    if len(candidates) == 0:
        raise CaptureError(f"No release build of '{app}' was found under '{buildPath}': build it, or use --exe")
    return max(candidates, key=lambda path: path.stat().st_mtime).resolve()


def FindWorkingDirectory(exePath: Path) -> Path:
    """An app looks for its 'Content' in the current directory, which the build places above the directory of the executable"""
    for directory in (exePath.parent, exePath.parent.parent):
        if (directory / "Content").is_dir():
            return directory
    return exePath.parent


class LoadProcess:
    """The external CPU load of a run, it runs while the object is in a 'with'"""

    def __init__(self, load: LoadConfig | None) -> None:
        self._load = load
        self._process: subprocess.Popen[bytes] | None = None

    def __enter__(self) -> "LoadProcess":
        if self._load is not None:
            command = [sys.executable, str(_g_toolDirectory / "CpuLoad.py"), "--processes", str(self._load.Processes), "--duty", str(self._load.Duty),
                       "--stop-on-stdin"]
            self._process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            time.sleep(_g_loadSettleSeconds)
            if self._process.poll() is not None:
                raise CaptureError("The CPU load stopped right after it was started")
        return self

    def __exit__(self, *args: object) -> None:
        if self._process is None:
            return
        try:
            # The load stops when its input ends, and its workers stop with it
            if self._process.stdin is not None:
                self._process.stdin.close()
            self._process.wait(_g_loadStopTimeoutSeconds)
        except (OSError, subprocess.TimeoutExpired):
            self._process.kill()
            self._process.wait()
        self._process = None


@dataclass
class RunResult:
    ExitCode: int
    TimedOut: bool
    CommandLine: list[str]
    StartTime: datetime.datetime
    EndTime: datetime.datetime
    # True if the tool made the window of the app topmost, False if it found no window to do that with, None where it can not do it
    WindowOnTop: bool | None = None


def RunApp(exePath: Path, workingDirectory: Path, logPath: Path, appOutputPath: Path, run: RunConfig, timeoutSeconds: float) -> RunResult:
    commandLine = [str(exePath), "--FramePacing.Log", str(logPath), "--ExitAfterFrame", str(run.Frames), "-v"] + run.Arguments
    startTime = datetime.datetime.now(datetime.timezone.utc)
    timedOut = False
    with LoadProcess(run.Load), open(appOutputPath, "wb") as appOutput:
        process = subprocess.Popen(commandLine, cwd=workingDirectory, stdin=subprocess.DEVNULL, stdout=appOutput, stderr=subprocess.STDOUT)
        # A window that opens behind another one is not shown, and then the run measures nothing
        windowOnTop = WindowOnTop(process.pid)
        try:
            exitCode = process.wait(timeoutSeconds)
        except subprocess.TimeoutExpired:
            timedOut = True
            process.kill()
            exitCode = process.wait()
        except KeyboardInterrupt:
            process.kill()
            process.wait()
            raise
        finally:
            windowOnTop.Stop()
    return RunResult(exitCode, timedOut, commandLine, startTime, datetime.datetime.now(datetime.timezone.utc),
                     windowOnTop.Done if WindowOnTop.IsSupported() else None)


def _WriteNotes(notesPath: Path, plan: Plan | None, run: RunConfig | None, result: RunResult | None, userFacts: dict[str, str],
                log: FramePacingLogFile, check: RunCheck, hardwareNames: bool) -> None:
    lines: list[str] = []
    if run is not None:
        lines.append(f"run: {run.Name}")
        lines.append(f"what: {run.What}")
    if plan is not None:
        lines.append(f"plan: {plan.SourcePath.name}: {plan.Description}")
        for key, value in plan.Variables.items():
            lines.append(f"plan variable: {key}={value}")
    if result is not None:
        lines.append(f"command line: {subprocess.list2cmdline(result.CommandLine)}")
        lines.append(f"started (UTC): {result.StartTime.isoformat(timespec='milliseconds')}")
        lines.append(f"ended (UTC): {result.EndTime.isoformat(timespec='milliseconds')}")
        lines.append(f"exit code: {result.ExitCode}" + (" (stopped by the tool, the run took too long)" if result.TimedOut else ""))
        if result.WindowOnTop is not None:
            lines.append("window: kept on top of the other windows by the tool" if result.WindowOnTop else
                         "window: THE TOOL FOUND NO WINDOW OF THE APP TO KEEP ON TOP, it can have been covered")
    if run is not None:
        lines.append(f"external load: {run.Load.Describe() if run.Load is not None else 'none'}")
    lines.append(f"log: {log.FramesPath.name} and {log.EventsPath.name}")
    lines.append("")
    lines.append("Told by the person at the machine (--fact):")
    lines.extend(f"  {key}: {value}" for key, value in userFacts.items())
    if len(userFacts) == 0:
        lines.append("  nothing")
    lines.append("")
    lines.append("The machine (read when the notes were written, right after the run):")
    lines.extend(f"  {line}" for line in _DescribeMachine(hardwareNames))
    lines.extend(f"  {line}" for line in _DescribeGraphics(log))
    lines.append("")
    lines.append("Checks (the first frames of the run are left out of the times):")
    lines.extend(f"  {line}" for line in FormatReport(check))
    notesPath.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _ParseKeyValues(entries: list[str] | None, option: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for entry in entries or []:
        key, separator, value = entry.partition("=")
        if separator == "" or key == "":
            raise CaptureError(f"{option} {entry}: expected key=value")
        result[key] = value
    return result


def _SelectRuns(plan: Plan, only: str | None) -> list[RunConfig]:
    if only is None:
        return plan.Runs
    wanted = [name for name in only.split(",") if name != ""]
    selected: list[RunConfig] = []
    for run in plan.Runs:
        baseName = run.Name.removesuffix(_g_idleSuffix).removesuffix(_g_loadedSuffix) if run.Loaded is not None else run.Name
        if run.Name in wanted or baseName in wanted:
            selected.append(run)
    missing = [name for name in wanted if not any(name in (run.Name, run.Name.removesuffix(_g_idleSuffix).removesuffix(_g_loadedSuffix))
                                                  for run in plan.Runs)]
    if len(missing) > 0:
        raise CaptureError(f"--only: the plan has no run named {', '.join(missing)}")
    return selected


def _CommandRun(args: argparse.Namespace) -> int:
    plan = LoadPlan(ResolvePlanPath(args.plan),
                    PlanOverrides(_ParseKeyValues(args.set, "--set"), args.frames, args.refresh_hz, args.load_processes, args.load_duty,
                                  args.external_load))
    userFacts = _ParseKeyValues(args.fact, "--fact")
    runs = _SelectRuns(plan, args.only)
    exePath = FindExecutable(plan.App, args.exe)
    workingDirectory = Path(args.cwd).resolve() if args.cwd is not None else FindWorkingDirectory(exePath)
    pauseSeconds = args.pause if args.pause is not None else plan.PauseSeconds
    outputPath = Path(args.output if args.output is not None else
                      f"FramePacingCapture/{datetime.datetime.now().strftime('%Y-%m-%d_%H%M%S')}_{plan.SourcePath.stem}").resolve()

    print(f"Plan: {plan.SourcePath.name}: {plan.Description}")
    print(f"App: {exePath}")
    print(f"Output: {outputPath}")
    for run in runs:
        print(f"  {run.Name}: {run.Frames} frames, {subprocess.list2cmdline(run.Arguments)}" + (", with CPU load" if run.Load is not None else ""))
    if args.dry_run:
        return 0

    existing = [run.Name for run in runs if (outputPath / f"{run.Name}.csv").exists()]
    if len(existing) > 0 and not args.overwrite:
        raise CaptureError(f"'{outputPath}' holds logs of these runs already: {', '.join(existing)} (use another --output, or --overwrite)")
    outputPath.mkdir(parents=True, exist_ok=True)

    if not args.yes:
        print("")
        print("Every run opens the window of the app. It takes the keyboard focus and the app reacts to keys, so leave the keyboard and the")
        print("mouse alone until the last run is done. Set the display mode and the variable refresh setting the plan is for first.")
        input("Press Enter to start (Ctrl+C to stop)...")

    summaryLines: list[str] = []
    warningCount = 0
    for index, run in enumerate(runs):
        print(f"[{index + 1}/{len(runs)}] {run.Name}: {run.What}", flush=True)
        time.sleep(pauseSeconds)
        logPath = outputPath / f"{run.Name}.csv"
        for stalePath in (logPath, ToEventsPath(logPath)):
            stalePath.unlink(missing_ok=True)
        result = RunApp(exePath, workingDirectory, logPath, outputPath / f"{run.Name}.app.log", run, plan.TimeoutSeconds)
        if not logPath.is_file():
            summary = f"{run.Name} | NO LOG, exit code {result.ExitCode}: see {run.Name}.app.log"
            warningCount += 1
        else:
            log = FramePacingLogFile(logPath)
            if not args.hardware_names:
                # The model of the GPU and the directories of this machine are taken out of what the app wrote, and the log is read
                # again so the notes do not name the model either
                AnonymiseFiles([logPath, ToEventsPath(logPath), outputPath / f"{run.Name}.app.log"], log.Facts, GetLocalPaths(outputPath))
                log = FramePacingLogFile(logPath)
            check = CheckRun(log, RunExpectation(run.Frames, run.RefreshRateHz, run.Loaded, result.ExitCode))
            notesPath = outputPath / f"{run.Name}.run.txt"
            _WriteNotes(notesPath, plan, run, result, userFacts, log, check, args.hardware_names)
            if not args.hardware_names:
                # The notes have the command line of the run
                AnonymiseFiles([notesPath], {}, GetLocalPaths(outputPath))
            summary = FormatSummary(run.Name, check)
            warningCount += len(check.Warnings)
            for warning in check.Warnings:
                summary += f"\n    WARNING: {warning}"
        print(f"  {summary}", flush=True)
        summaryLines.append(summary)

    with open(outputPath / "summary.txt", "a", encoding="utf-8") as file:
        file.write(f"{datetime.datetime.now(datetime.timezone.utc).isoformat(timespec='seconds')} {plan.SourcePath.name}: {plan.Description}\n")
        file.write("\n".join(summaryLines) + "\n\n")
    print(f"Done: {len(runs)} runs, {warningCount} warnings. The notes of a run are in <run>.run.txt, all summaries in summary.txt")
    return 0 if warningCount == 0 else 1


def _CommandCheck(args: argparse.Namespace) -> int:
    userFacts = _ParseKeyValues(args.fact, "--fact")
    warningCount = 0
    for logArgument in args.logs:
        logPath = Path(logArgument)
        if not logPath.is_file():
            raise CaptureError(f"'{logPath}' is not a file")
        log = FramePacingLogFile(logPath)
        check = CheckRun(log, RunExpectation(RefreshRateHz=args.refresh_hz))
        warningCount += len(check.Warnings)
        print(FormatSummary(logPath.stem, check))
        if args.write_notes:
            notesPath = logPath.with_name(logPath.stem + ".run.txt")
            _WriteNotes(notesPath, None, None, None, userFacts, log, check, args.hardware_names)
            if not args.hardware_names:
                AnonymiseFiles([notesPath], {}, GetLocalPaths(logPath.parent))
        else:
            for line in _DescribeGraphics(log) + FormatReport(check):
                print(f"  {line}")
    return 0 if warningCount == 0 else 1


def _CommandList(args: argparse.Namespace) -> int:
    if args.plan is None:
        for path in sorted(_g_planDirectory.glob("*.toml")):
            with open(path, "rb") as file:
                print(f"{path.stem}: {tomllib.load(file).get('description', '')}")
        return 0
    plan = LoadPlan(ResolvePlanPath(args.plan), PlanOverrides())
    print(f"{plan.SourcePath.name}: {plan.Description}")
    for key, value in plan.Variables.items():
        print(f"  variable {key}={value}")
    for run in plan.Runs:
        print(f"  {run.Name}: {run.What}: {run.Frames} frames, {subprocess.list2cmdline(run.Arguments)}")
    return 0


def _InterpolateSteps(points: list[tuple[int, float]], targetMs: float) -> str:
    """The GPU load setting that gives the wanted GPU time, from the measured (steps, milliseconds) points"""
    if targetMs < points[0][1]:
        return f"below {points[0][0]}"
    for (lowSteps, lowMs), (highSteps, highMs) in zip(points, points[1:]):
        if lowMs <= targetMs <= highMs and highMs > lowMs:
            return str(round(lowSteps + (highSteps - lowSteps) * (targetMs - lowMs) / (highMs - lowMs)))
    return f"above {points[-1][0]}"


def _CommandCalibrate(args: argparse.Namespace) -> int:
    exePath = FindExecutable(args.app, args.exe)
    workingDirectory = Path(args.cwd).resolve() if args.cwd is not None else FindWorkingDirectory(exePath)
    outputPath = Path(args.output if args.output is not None else
                      f"FramePacingCapture/{datetime.datetime.now().strftime('%Y-%m-%d_%H%M%S')}_calibrate").resolve()
    stepValues = sorted({int(value) for value in args.steps.split(",")})
    percentValues = [float(value) for value in args.percent.split(",")]
    outputPath.mkdir(parents=True, exist_ok=True)
    print(f"App: {exePath}")
    print(f"Window: {args.window}, {args.frames} frames for each of {len(stepValues)} GPU load settings. The window takes the keyboard focus.")

    points: list[tuple[int, float]] = []
    refreshIntervalTicks: int | None = round(g_ticksPerSecond / args.refresh_hz) if args.refresh_hz is not None else None
    for steps in stepValues:
        time.sleep(args.pause)
        run = RunConfig(f"gpu_{steps}", "", ["--GpuLoad", str(steps), "--Window", args.window], args.frames, None, None, args.refresh_hz)
        logPath = outputPath / f"{run.Name}.csv"
        result = RunApp(exePath, workingDirectory, logPath, outputPath / f"{run.Name}.app.log", run, _g_defaultTimeoutSeconds)
        if result.ExitCode != 0 or not logPath.is_file():
            raise CaptureError(f"The run with a GPU load of {steps} failed (exit code {result.ExitCode}): see {run.Name}.app.log in {outputPath}")
        if not args.hardware_names:
            AnonymiseFiles([logPath, ToEventsPath(logPath), outputPath / f"{run.Name}.app.log"], FramePacingLogFile(logPath).Facts,
                           GetLocalPaths(outputPath))
        check = CheckRun(FramePacingLogFile(logPath), RunExpectation(RefreshRateHz=args.refresh_hz))
        if check.WorkGpuMs is None:
            raise CaptureError("The log has no GPU time of the frames (the app has to be a FramePacing sample that can time the GPU)")
        if refreshIntervalTicks is None:
            refreshIntervalTicks = check.RefreshIntervalTicks
        points.append((steps, check.WorkGpuMs.Median))
        print(f"  --GpuLoad {steps}: the GPU works {check.WorkGpuMs.Median:.3f} ms on a frame", flush=True)

    if refreshIntervalTicks is None:
        raise CaptureError("The refresh rate of the display is not known: use --refresh-hz")
    refreshMs = refreshIntervalTicks / g_ticksPerMillisecond
    print(f"A refresh takes {refreshMs:.4f} ms ({g_ticksPerSecond / refreshIntervalTicks:.3f} Hz)")
    settings: list[str] = []
    for percent in percentValues:
        steps = _InterpolateSteps(points, refreshMs * percent / 100.0)
        print(f"  {percent:g} % of a refresh ({refreshMs * percent / 100.0:.3f} ms): --GpuLoad {steps}")
        settings.append(f"--set gpu{percent:g}={steps}")
    print(f"For a plan with these variables: {' '.join(settings)}")
    return 0


def _CreateParser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Captures frame pacing logs, see Doc/FramePacingCapture.md.")
    commands = parser.add_subparsers(dest="command", required=True)

    run = commands.add_parser("run", help="Capture the runs of a plan.")
    run.add_argument("plan", help="A plan file, or the name of one of the plans that come with the tool (see 'list').")
    run.add_argument("-o", "--output", help="The directory for the logs and notes (default: FramePacingCapture/<date>_<plan> in the current one).")
    run.add_argument("--exe", help=f"The app to run (default: the newest release build of the plan's app under {_g_sdkEnvironmentVariable}/build).")
    run.add_argument("--cwd", help="The directory to run the app in (default: the one above the executable that holds its Content).")
    run.add_argument("--only", help="Only these runs, separated by commas (the name of a loaded run selects its idle and loaded pair).")
    run.add_argument("--set", action="append", metavar="NAME=VALUE", help="Set a variable of the plan (can be given more than once).")
    run.add_argument("--fact", action="append", metavar="KEY=VALUE",
                     help="Something only you know, written to the notes of every run: --fact gsync=off (can be given more than once).")
    run.add_argument("--frames", type=int, help="The number of frames of every run, instead of the ones of the plan.")
    run.add_argument("--pause", type=float, help="The seconds to wait before every run, instead of the ones of the plan.")
    run.add_argument("--refresh-hz", type=float, help="The refresh rate the display is set to, the runs are checked against it.")
    run.add_argument("--load-processes", type=int, help="The busy processes of the CPU load, instead of the ones of the plan (0 = all CPUs).")
    run.add_argument("--load-duty", type=float, help="The share of the time the CPU load is busy, instead of the one of the plan (0.05 to 1.0).")
    run.add_argument("--external-load", action="store_true",
                     help="The machine is under a load you started yourself (a build, for example): every loaded run is run once, "
                          "without an idle twin and without the CPU load of the tool. Say what the load is with --fact.")
    run.add_argument("--overwrite", action="store_true", help="Replace the logs of runs the output directory holds already.")
    run.add_argument("--hardware-names", action="store_true",
                     help="Keep the model of the GPU and of the CPU and the directories of this machine in the logs and the notes. Without it "
                          "the tool replaces the model of the GPU by its vendor and the directories by <output>, <sdk> and <home> in every "
                          "file of a run and leaves the model of the CPU out of the notes, so a capture can be handed on without naming "
                          "the machine it was made on.")
    run.add_argument("--dry-run", action="store_true", help="Show the runs and stop.")
    run.add_argument("-y", "--yes", action="store_true", help="Start without asking.")
    run.set_defaults(function=_CommandRun)

    check = commands.add_parser("check", help="Check logs that exist already.")
    check.add_argument("logs", nargs="+", help="The frames file of a log (the events file is found next to it).")
    check.add_argument("--refresh-hz", type=float, help="The refresh rate the display was meant to run at.")
    check.add_argument("--write-notes", action="store_true", help="Write <log>.run.txt next to every log instead of printing the checks.")
    check.add_argument("--fact", action="append", metavar="KEY=VALUE", help="Something only you know, for the notes (can be given more than once).")
    check.add_argument("--hardware-names", action="store_true",
                       help="Name the model of the CPU and the directories of this machine in the notes. The logs are checked as they "
                            "are: a log that was captured with --hardware-names still names its GPU.")
    check.set_defaults(function=_CommandCheck)

    listCommand = commands.add_parser("list", help="List the plans that come with the tool, or the runs of a plan.")
    listCommand.add_argument("plan", nargs="?", help="A plan file or name.")
    listCommand.set_defaults(function=_CommandList)

    calibrate = commands.add_parser("calibrate", help="Find the GPU load settings that make the GPU work a share of a refresh on every frame.")
    calibrate.add_argument("--app", default=_g_defaultApp, help=f"The FramePacing sample to use (default {_g_defaultApp}).")
    calibrate.add_argument("--exe", help="The app to run, instead of the newest release build of --app.")
    calibrate.add_argument("--cwd", help="The directory to run the app in.")
    calibrate.add_argument("--window", default=_g_defaultCalibrateWindow,
                           help=f"The window of the runs, the GPU time depends on its size (default {_g_defaultCalibrateWindow}, [] is fullscreen).")
    calibrate.add_argument("--steps", default=_g_defaultCalibrateSteps, help=f"The GPU load settings to measure (default {_g_defaultCalibrateSteps}).")
    calibrate.add_argument("--percent", default=_g_defaultCalibratePercent,
                           help=f"The shares of a refresh to find the setting for (default {_g_defaultCalibratePercent}).")
    calibrate.add_argument("--frames", type=int, default=400, help="The number of frames of a run (default 400).")
    calibrate.add_argument("--refresh-hz", type=float, help="The refresh rate of the display, if the window system does not report it.")
    calibrate.add_argument("--pause", type=float, default=1.0, help="The seconds to wait before every run (default 1).")
    calibrate.add_argument("-o", "--output", help="The directory for the logs of the runs.")
    calibrate.add_argument("--hardware-names", action="store_true", help="Keep the model of the GPU and the directories of this machine in the logs of the runs.")
    calibrate.set_defaults(function=_CommandCalibrate)
    return parser


def Main(argv: list[str]) -> int:
    args = _CreateParser().parse_args(argv)
    try:
        return int(args.function(args))
    except CaptureError as ex:
        print(f"ERROR: {ex}", file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        print("Stopped", file=sys.stderr)
        return 130


if __name__ == "__main__":
    sys.exit(Main(sys.argv[1:]))
