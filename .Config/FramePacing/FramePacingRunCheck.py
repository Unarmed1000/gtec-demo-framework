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

# Reduces a frame pacing log to the numbers that tell if a run is usable and what it showed, and to the warnings that tell it is not what it was
# meant to be (the pacer was switched during the run, the display runs at another rate, the machine was busy during a run that was meant to be
# idle, ...). The numbers are a first look, the log holds the values they are made from.

from collections import Counter
from dataclasses import dataclass, field

from FramePacingLogFile import FramePacingLogFile, g_ticksPerMillisecond, g_ticksPerSecond

# The first frames of a run are left out of the timing numbers: the swapchain, the pacer and the GPU timer are still settling
_g_skippedStartFrames = 60
# The measurements of the last presents were not read when the app exited
_g_unreadEndFrames = 16
_g_refreshRateToleranceHz = 0.5
# How far the refresh the swapchain reports may be from the one of the window system, as a share of it
_g_swapchainRefreshTolerance = 0.02
_g_minTimedPresentShare = 0.9
# The share of the CPUs other programs may use during a run that is meant to be idle
_g_idleOtherBusyLimit = 0.10
# The share of the CPUs other programs have to use for a run with load to count as loaded
_g_loadedOtherBusyMinimum = 0.10
# The share of the display times the pacer may refuse before its present feedback counts as of no use
_g_feedbackRefusedLimit = 0.10
_g_sourceWindowSystem = "the window system"
# The names of the values of the holdMethod column
_g_holdMethodNames = {0: "sleep", 1: "vsync wait", 2: "present again", 3: "scheduled present"}
_g_sourcePacer = "the settings of the pacer"
_g_sourcePlan = "the plan"


@dataclass
class RunExpectation:
    """What the run was meant to be, None where nothing was asked for"""
    Frames: int | None = None
    RefreshRateHz: float | None = None
    Loaded: bool | None = None
    ExitCode: int | None = None


@dataclass
class Distribution:
    Median: float
    P1: float
    P99: float
    Count: int


@dataclass
class RunCheck:
    Rows: int = 0
    RefreshIntervalTicks: int | None = None
    RefreshIntervalSource: str = "not known"
    SwapchainCount: int | None = None
    PacerOnRows: int | None = None
    PacerOffRows: int | None = None
    SwapIntervals: dict[int, int] = field(default_factory=dict)
    PacerChanges: dict[str, int] = field(default_factory=dict)
    TimedPresents: int | None = None
    # Presents the presentation engine reported on without a display time: their image did not reach the display, or it was not timed
    UntimedReports: int = 0
    # Presents that were not asked to be timed, None if the log does not tell
    UnrequestedPresents: int | None = None
    FrameStartColumn: str = ""
    FrameStartIntervalMs: Distribution | None = None
    DisplayIntervalMs: Distribution | None = None
    ShownForRefreshes: dict[int, int] = field(default_factory=dict)
    LatencyMs: Distribution | None = None
    # The display time of a frame minus the display time its pacer intended
    DisplayErrorMs: Distribution | None = None
    # The frames by how they were held for their swap interval (the names of _g_holdMethodNames), empty if the log does not tell
    HoldMethods: dict[str, int] = field(default_factory=dict)
    # The frames the pacer measured by their display times, None if the log does not tell
    FeedbackOnRows: int | None = None
    # What became of the present feedback the pacer was given: used, refused, notShown, missing, lateRefreshes
    FeedbackState: dict[str, int] = field(default_factory=dict)
    ResultFramesLate: dict[int, int] = field(default_factory=dict)
    WorkCpuMs: Distribution | None = None
    WorkGpuMs: Distribution | None = None
    SystemBusyShare: float | None = None
    ProcessCpuShare: float | None = None
    ProcessCpuCores: float | None = None
    ProcessGpuPercent: float | None = None
    ProcessGpuDedicatedBytes: int | None = None
    LoadSamples: int = 0
    # The refresh the swapchain reported first (VK_EXT_present_timing), in nanoseconds
    SwapchainRefreshNs: float | None = None
    Warnings: list[str] = field(default_factory=list)

    def GetRefreshRateHz(self) -> float | None:
        return g_ticksPerSecond / self.RefreshIntervalTicks if self.RefreshIntervalTicks else None

    def GetOtherBusyShare(self) -> float | None:
        """The share of the CPUs that was busy with something other than the app"""
        if self.SystemBusyShare is None or self.ProcessCpuShare is None:
            return None
        return max(self.SystemBusyShare - self.ProcessCpuShare, 0.0)


def _Percentile(sortedValues: list[float], share: float) -> float:
    index = min(max(round(share * (len(sortedValues) - 1)), 0), len(sortedValues) - 1)
    return sortedValues[index]


def _ToDistributionMs(ticks: list[int]) -> Distribution | None:
    if len(ticks) == 0:
        return None
    sortedMs = sorted(value / g_ticksPerMillisecond for value in ticks)
    return Distribution(_Percentile(sortedMs, 0.5), _Percentile(sortedMs, 0.01), _Percentile(sortedMs, 0.99), len(sortedMs))


def _Deltas(column: list[int | None], firstRow: int) -> list[int]:
    """The steps between rows that follow each other and both have a value"""
    result: list[int] = []
    for index in range(max(firstRow, 1), len(column)):
        current = column[index]
        previous = column[index - 1]
        if current is not None and previous is not None:
            result.append(current - previous)
    return result


def _DeltasOfValues(column: list[int | None], firstRow: int) -> list[int]:
    """The steps from one value to the next value, the rows without a value are passed over"""
    values = [value for value in column[firstRow:] if value is not None]
    return [current - previous for previous, current in zip(values, values[1:])]


def _FindRefreshInterval(log: FramePacingLogFile, expectation: RunExpectation) -> tuple[int | None, str]:
    for event in reversed(log.GetEvents("display")):
        ticks = int(event.GetValues().get("refreshIntervalTicks", "0"))
        if ticks > 0:
            return ticks, _g_sourceWindowSystem
    for event in reversed(log.GetEvents("pacerConfig")):
        rateHz = float(event.GetValues().get("refreshRateHz", "0"))
        if rateHz > 0.0:
            return round(g_ticksPerSecond / rateHz), _g_sourcePacer
    if expectation.RefreshRateHz:
        return round(g_ticksPerSecond / expectation.RefreshRateHz), _g_sourcePlan
    return None, "not known"


def _CheckLoad(log: FramePacingLogFile, check: RunCheck) -> None:
    idle = log.GetColumn("systemIdleTicks")
    kernel = log.GetColumn("systemKernelTicks")
    user = log.GetColumn("systemUserTicks")
    processKernel = log.GetColumn("processKernelTicks")
    processUser = log.GetColumn("processUserTicks")
    wallClock = log.GetColumn("beginFrameTicks")
    rows = [index for index in range(log.RowCount) if idle[index] is not None and kernel[index] is not None and user[index] is not None]
    check.LoadSamples = len(rows)
    if len(rows) >= 2:
        first = rows[0]
        last = rows[-1]

        def Step(column: list[int | None]) -> int:
            firstValue = column[first]
            lastValue = column[last]
            return lastValue - firstValue if firstValue is not None and lastValue is not None else 0

        busyTicks = Step(kernel) + Step(user)
        totalTicks = busyTicks + Step(idle)
        processTicks = Step(processKernel) + Step(processUser)
        if totalTicks > 0:
            check.SystemBusyShare = busyTicks / totalTicks
            check.ProcessCpuShare = processTicks / totalTicks
        wallTicks = Step(wallClock)
        if wallTicks > 0:
            check.ProcessCpuCores = processTicks / wallTicks
    gpuUsage = log.GetValues("processGpuUsageMilliPercent")
    if len(gpuUsage) > 0:
        check.ProcessGpuPercent = (sum(gpuUsage) / len(gpuUsage)) / 1000.0
    gpuDedicated = log.GetValues("processGpuDedicatedBytes")
    if len(gpuDedicated) > 0:
        check.ProcessGpuDedicatedBytes = max(gpuDedicated)


def _AddWarnings(log: FramePacingLogFile, check: RunCheck, expectation: RunExpectation) -> None:
    warnings = check.Warnings
    if expectation.ExitCode is not None and expectation.ExitCode != 0:
        warnings.append(f"the app exited with code {expectation.ExitCode}")
    if expectation.Frames is not None and check.Rows != expectation.Frames:
        warnings.append(f"{check.Rows} frames were logged, {expectation.Frames} were asked for")
    if check.SwapchainCount is not None and check.SwapchainCount != 1:
        warnings.append(f"{check.SwapchainCount} swapchains were created (the window was resized, moved to another display or lost)")
    if check.PacerOnRows and check.PacerOffRows:
        warnings.append(f"the pacer was on for {check.PacerOnRows} frames and off for {check.PacerOffRows}: it was switched during the run "
                        "(the window has the keyboard focus and P toggles the pacer)")
    rateHz = check.GetRefreshRateHz()
    if expectation.RefreshRateHz is not None:
        if rateHz is None or check.RefreshIntervalSource == _g_sourcePlan:
            warnings.append("the refresh rate of the display is not in the log, the one of the plan was used")
        elif abs(rateHz - expectation.RefreshRateHz) > _g_refreshRateToleranceHz:
            warnings.append(f"the display runs at {rateHz:.3f} Hz, the plan is for {expectation.RefreshRateHz:g} Hz")
    if len(log.GetEvents("display")) > 1:
        warnings.append("the refresh rate of the display changed during the run")
    for event in log.GetEvents("refreshProperties"):
        swapchainNs = float(event.GetValues().get("refreshDurationNs", "0"))
        if swapchainNs > 0.0:
            check.SwapchainRefreshNs = swapchainNs
            break
    if check.SwapchainRefreshNs and check.RefreshIntervalTicks and check.RefreshIntervalSource == _g_sourceWindowSystem:
        # The swapchain reports a refresh of its own (VK_EXT_present_timing). A shorter one than the window system's is what a window
        # gets on a desktop with a faster display next to its own, a longer one means the display is not in the state the run is for
        windowSystemNs = check.RefreshIntervalTicks * 100.0
        if check.SwapchainRefreshNs > windowSystemNs * (1.0 + _g_swapchainRefreshTolerance):
            warnings.append(f"the swapchain reports a refresh of {check.SwapchainRefreshNs / 1000000.0:.3f} ms, the window system one of "
                            f"{windowSystemNs / 1000000.0:.3f} ms: variable refresh is on, or something else has the display")
    presentTiming = log.GetEvents("presentTiming")
    if len(presentTiming) > 0 and presentTiming[-1].GetValues().get("enabled") == "1" and check.TimedPresents is not None:
        expected = max(check.Rows - _g_unreadEndFrames, 1)
        if check.TimedPresents < expected * _g_minTimedPresentShare:
            unrequested = f", {check.UnrequestedPresents} were not asked to be timed" if check.UnrequestedPresents is not None else ""
            warnings.append(f"only {check.TimedPresents} of {check.Rows} presents have a display time, {check.UntimedReports} were reported "
                            f"without one (their image did not reach the display, or it was not timed){unrequested}")
    used = check.FeedbackState.get("used", 0)
    refused = check.FeedbackState.get("refused", 0)
    if check.FeedbackOnRows and (used + refused) > 0 and refused > (used + refused) * _g_feedbackRefusedLimit:
        warnings.append(f"the pacer refused {refused} of {used + refused} display times: variable refresh is on, or the refresh rate it "
                        "paces at is not the one of the display")
    otherBusy = check.GetOtherBusyShare()
    if expectation.Loaded is not None:
        if otherBusy is None:
            warnings.append("the log has no load of the machine (no system stats on this platform, or the run was shorter than two seconds)")
        elif not expectation.Loaded and otherBusy > _g_idleOtherBusyLimit:
            warnings.append(f"other programs used {otherBusy * 100.0:.0f} % of the CPUs during a run that was meant to be idle")
        elif expectation.Loaded and otherBusy < _g_loadedOtherBusyMinimum:
            warnings.append(f"other programs used {otherBusy * 100.0:.0f} % of the CPUs during a run that was meant to be loaded")


def CheckRun(log: FramePacingLogFile, expectation: RunExpectation) -> RunCheck:
    check = RunCheck()
    check.Rows = log.RowCount
    check.RefreshIntervalTicks, check.RefreshIntervalSource = _FindRefreshInterval(log, expectation)

    swapchains = log.GetEvents("swapchainCreated")
    if len(swapchains) > 0:
        check.SwapchainCount = len(swapchains)

    if log.HasColumn("pacerOn"):
        pacerOn = log.GetValues("pacerOn")
        check.PacerOnRows = sum(1 for value in pacerOn if value != 0)
        check.PacerOffRows = len(pacerOn) - check.PacerOnRows
        check.SwapIntervals = dict(sorted(Counter(log.GetValues("swapInterval")).items()))
        changes = Counter(log.GetValues("pacerChange"))
        check.PacerChanges = {"slower": changes.get(1, 0), "faster": changes.get(2, 0)}

    if log.HasColumn("holdMethod"):
        counts = Counter(log.GetValues("holdMethod"))
        check.HoldMethods = {_g_holdMethodNames.get(code, f"method {code}"): count for code, count in sorted(counts.items())}
    if log.HasColumn("pacerFeedbackOn"):
        check.FeedbackOnRows = sum(1 for value in log.GetValues("pacerFeedbackOn") if value != 0)
        for key, column in (("used", "pacerFeedbackUsed"), ("refused", "pacerFeedbackRefused"), ("notShown", "pacerFeedbackNotShown"),
                            ("missing", "pacerFeedbackMissing"), ("lateRefreshes", "pacerFeedbackLateRefreshes")):
            values = log.GetValues(column)
            if len(values) > 0:
                # The counters only grow while a pacer lives, a new pacer starts them again
                check.FeedbackState[key] = values[-1]

    firstRow = min(_g_skippedStartFrames, log.RowCount // 4)
    check.FrameStartColumn = "frameStartTicks" if log.HasColumn("frameStartTicks") else "cpuStartTicks"
    frameStart = log.GetColumn(check.FrameStartColumn)
    check.FrameStartIntervalMs = _ToDistributionMs(_Deltas(frameStart, firstRow))

    if log.HasColumn("firstPixelOutTicks"):
        firstPixelOut = log.GetColumn("firstPixelOutTicks")
        check.TimedPresents = sum(1 for value in firstPixelOut if value is not None)
        check.UntimedReports = sum(1 for read, shown in zip(log.GetColumn("resultReadAtFrame"), firstPixelOut) if read is not None and shown is None)
        if log.HasColumn("presentTimingRequested"):
            check.UnrequestedPresents = sum(1 for value in log.GetValues("presentTimingRequested") if value == 0)
        # From one image that reached the display to the next one that did
        displaySteps = _DeltasOfValues(firstPixelOut, firstRow)
        check.DisplayIntervalMs = _ToDistributionMs(displaySteps)
        if check.RefreshIntervalTicks:
            refreshes = Counter(round(step / check.RefreshIntervalTicks) for step in displaySteps)
            check.ShownForRefreshes = dict(sorted(refreshes.items()))
        latency = [shown - start for shown, start in zip(firstPixelOut[firstRow:], frameStart[firstRow:]) if shown is not None and start is not None]
        check.LatencyMs = _ToDistributionMs(latency)
        intended = log.GetColumn("intendedDisplayTicks")[firstRow:]
        # An intended display time of zero is one the pacer does not know
        check.DisplayErrorMs = _ToDistributionMs([shown - aimed for shown, aimed in zip(firstPixelOut[firstRow:], intended)
                                                  if shown is not None and aimed is not None and aimed != 0])
        frameIndex = log.GetColumn("frameIndex")
        late = Counter(read - index for read, index in zip(log.GetColumn("resultReadAtFrame"), frameIndex) if read is not None and index is not None)
        check.ResultFramesLate = dict(sorted(late.items()))

    check.WorkCpuMs = _ToDistributionMs([value for value in log.GetColumn("workCpuTicks")[firstRow:] if value is not None])
    if log.HasColumn("gpuWorkBeginTicks") and log.HasColumn("gpuWorkEndTicks"):
        begin = log.GetColumn("gpuWorkBeginTicks")[firstRow:]
        end = log.GetColumn("gpuWorkEndTicks")[firstRow:]
        check.WorkGpuMs = _ToDistributionMs([endTime - beginTime for beginTime, endTime in zip(begin, end)
                                             if beginTime is not None and endTime is not None])
    else:
        check.WorkGpuMs = _ToDistributionMs([value for value in log.GetColumn("workGpuTicks")[firstRow:] if value is not None])

    _CheckLoad(log, check)
    _AddWarnings(log, check, expectation)
    return check


def _FormatCounts(counts: dict[int, int] | dict[str, int]) -> str:
    return ", ".join(f"{key}: {value}" for key, value in counts.items()) if len(counts) > 0 else "none"


def _FormatDistribution(distribution: Distribution | None, refreshIntervalTicks: int | None, asShareOfRefresh: bool = False) -> str:
    if distribution is None:
        return "no values"
    text = f"median {distribution.Median:.3f} ms, 1 % under {distribution.P1:.3f} ms, 1 % over {distribution.P99:.3f} ms ({distribution.Count} frames)"
    if refreshIntervalTicks:
        refreshMs = refreshIntervalTicks / g_ticksPerMillisecond
        if asShareOfRefresh:
            text += f", the median is {distribution.Median / refreshMs * 100.0:.0f} % of a refresh"
        else:
            text += f", the median is {distribution.Median / refreshMs:.2f} refreshes"
    return text


def FormatReport(check: RunCheck) -> list[str]:
    """The lines for the notes of a run"""
    rateHz = check.GetRefreshRateHz()
    lines = [f"frames logged: {check.Rows}"]
    if rateHz is not None and check.RefreshIntervalTicks is not None:
        lines.append(f"display refresh: {rateHz:.4f} Hz ({check.RefreshIntervalTicks} ticks of 100 ns), from {check.RefreshIntervalSource}")
    else:
        lines.append("display refresh: not known")
    if check.SwapchainRefreshNs:
        lines.append(f"refresh the swapchain reports: {check.SwapchainRefreshNs / 1000000.0:.3f} ms (with displays at different rates it is "
                     "the one of the fastest display)")
    if check.SwapchainCount is not None:
        lines.append(f"swapchains created: {check.SwapchainCount}")
    if check.PacerOnRows is not None:
        lines.append(f"pacer: on for {check.PacerOnRows} frames, off for {check.PacerOffRows}")
        lines.append(f"swap interval of the paced frames (interval: frames): {_FormatCounts(check.SwapIntervals)}")
        lines.append(f"swap interval changes of the pacer: {_FormatCounts(check.PacerChanges)}")
        if len(check.HoldMethods) > 0:
            lines.append(f"how the frames were held for their swap interval (method: frames): {_FormatCounts(check.HoldMethods)}")
        if check.FeedbackOnRows is not None:
            state = f", the display times at the end of the run: {_FormatCounts(check.FeedbackState)}" if check.FeedbackOnRows > 0 else ""
            lines.append(f"present feedback to the pacer: on for {check.FeedbackOnRows} frames{state}")
    lines.append(f"frame start to frame start ({check.FrameStartColumn}): {_FormatDistribution(check.FrameStartIntervalMs, check.RefreshIntervalTicks)}")
    if check.TimedPresents is not None:
        lines.append(f"presents with a display time: {check.TimedPresents} of {check.Rows}")
        lines.append(f"presents that were reported without a display time: {check.UntimedReports}")
        if check.UnrequestedPresents is not None:
            lines.append(f"presents that were not asked to be timed (too many results were outstanding): {check.UnrequestedPresents}")
        lines.append(f"display time to the next display time: {_FormatDistribution(check.DisplayIntervalMs, check.RefreshIntervalTicks)}")
        lines.append(f"frames by the refreshes they were shown for (refreshes: frames): {_FormatCounts(check.ShownForRefreshes)}")
        lines.append(f"frame start to display time: {_FormatDistribution(check.LatencyMs, check.RefreshIntervalTicks)}")
        if check.DisplayErrorMs is not None:
            lines.append(f"display time minus the intended display time: {_FormatDistribution(check.DisplayErrorMs, check.RefreshIntervalTicks)}")
        lines.append(f"frames a display time arrived late (frames: presents): {_FormatCounts(check.ResultFramesLate)}")
    else:
        lines.append("presents with a display time: none (no present timing)")
    if check.WorkCpuMs is not None:
        lines.append(f"CPU work of a frame: {_FormatDistribution(check.WorkCpuMs, check.RefreshIntervalTicks, True)}")
    if check.WorkGpuMs is not None:
        lines.append(f"GPU work of a frame: {_FormatDistribution(check.WorkGpuMs, check.RefreshIntervalTicks, True)}")
    if check.SystemBusyShare is not None and check.ProcessCpuShare is not None:
        otherBusy = check.GetOtherBusyShare()
        cores = f", {check.ProcessCpuCores:.2f} CPUs" if check.ProcessCpuCores is not None else ""
        lines.append(f"machine load: the CPUs were busy {check.SystemBusyShare * 100.0:.1f} % of the time, the app used "
                     f"{check.ProcessCpuShare * 100.0:.1f} %{cores}, other programs {(otherBusy or 0.0) * 100.0:.1f} % ({check.LoadSamples} samples)")
    else:
        lines.append("machine load: not in the log")
    if check.ProcessGpuPercent is not None:
        memory = f", {check.ProcessGpuDedicatedBytes / (1024 * 1024):.0f} MB of GPU memory" if check.ProcessGpuDedicatedBytes is not None else ""
        lines.append(f"GPU load of the app: {check.ProcessGpuPercent:.1f} %{memory}")
    if len(check.Warnings) == 0:
        lines.append("warnings: none")
    for warning in check.Warnings:
        lines.append(f"WARNING: {warning}")
    return lines


def FormatSummary(name: str, check: RunCheck) -> str:
    """One line for the summary of a capture"""
    parts = [name, f"{check.Rows} frames"]
    rateHz = check.GetRefreshRateHz()
    parts.append(f"{rateHz:.2f} Hz" if rateHz is not None else "? Hz")
    if check.PacerOnRows is not None:
        if check.PacerOnRows > 0 and not check.PacerOffRows:
            parts.append(f"pacer on, swap interval {_FormatCounts(check.SwapIntervals)}")
        elif check.PacerOnRows == 0:
            parts.append("pacer off")
        else:
            parts.append("pacer SWITCHED")
        if len(check.HoldMethods) == 1:
            parts.append(f"hold: {next(iter(check.HoldMethods))}")
        elif len(check.HoldMethods) > 1:
            parts.append(f"hold: {_FormatCounts(check.HoldMethods)}")
        if check.FeedbackOnRows:
            parts.append(f"feedback {_FormatCounts(check.FeedbackState)}")
    if check.FrameStartIntervalMs is not None:
        parts.append(f"start step {check.FrameStartIntervalMs.Median:.3f} ms ({check.FrameStartIntervalMs.P1:.3f} to {check.FrameStartIntervalMs.P99:.3f})")
    if len(check.ShownForRefreshes) > 0:
        parts.append(f"shown for refreshes {_FormatCounts(check.ShownForRefreshes)}")
        if check.UntimedReports > 0:
            parts.append(f"{check.UntimedReports} presents not shown or not timed")
    elif check.DisplayIntervalMs is not None:
        parts.append(f"display step {check.DisplayIntervalMs.Median:.3f} ms")
    if check.WorkCpuMs is not None and check.WorkGpuMs is not None:
        parts.append(f"work cpu {check.WorkCpuMs.Median:.2f} ms gpu {check.WorkGpuMs.Median:.2f} ms")
    otherBusy = check.GetOtherBusyShare()
    if otherBusy is not None:
        parts.append(f"other programs {otherBusy * 100.0:.0f} % cpu")
    parts.append("ok" if len(check.Warnings) == 0 else f"{len(check.Warnings)} WARNINGS")
    return " | ".join(parts)
