#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2024 NXP
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#    * Redistributions of source code must retain the above copyright notice,
#      this list of conditions and the following disclaimer.
#
#    * Redistributions in binary form must reproduce the above copyright notice,
#      this list of conditions and the following disclaimer in the documentation
#      and/or other materials provided with the distribution.
#
#    * Neither the name of the NXP. nor the names of
#      its contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# ****************************************************************************************************************************************************

from collections.abc import Iterable
from enum import Enum


class ForAllMode(Enum):
    RunExe = 0
    RunCustom = 1


class ForAllConfig:
    @staticmethod
    def CreateForAllExeConfig(forAllExe: str, filterFeatureNameList: list[str] | None = None) -> ForAllConfig | None:
        return ForAllConfig(ForAllMode.RunExe, forAllExe, filterFeatureNameList)

    @staticmethod
    def CreateForAllPackagesConfig(forAll: str, filterFeatureNameList: list[str] | None = None) -> ForAllConfig | None:
        return ForAllConfig(ForAllMode.RunCustom, forAll, filterFeatureNameList)

    @staticmethod
    def CreateForAllExeArgumentsConfig(
        runArguments: list[str], timeoutSeconds: float | None = None, runPackageNames: Iterable[str] | None = None
    ) -> ForAllConfig:
        """Run a command for each executable. The command is given as its arguments, so it is not split: an argument can hold a space.
        The '(EXE)' kind of names are replaced in each argument. timeoutSeconds stops a command that does not return in time.
        runPackageNames: the command is only run for the packages with these names, the other ones are only built.
        """
        config = ForAllConfig(ForAllMode.RunExe, " ".join(runArguments))
        config.RunArguments = list(runArguments)
        config.TimeoutSeconds = timeoutSeconds
        config.RunPackageNames = None if runPackageNames is None else set(runPackageNames)
        return config

    def __init__(self, mode: ForAllMode, runCommand: str, filterFeatureNameList: list[str] | None = None) -> None:
        super().__init__()
        self.Mode = mode
        self.RunCommand = runCommand
        # The command as arguments, None: RunCommand is split into its arguments
        self.RunArguments: list[str] | None = None
        # The time a command gets, None: as long as it takes
        self.TimeoutSeconds: float | None = None
        # The names of the packages the command is run for, None: every package
        self.RunPackageNames: set[str] | None = None
        self.FilterFeatureNameList = [] if filterFeatureNameList is None else filterFeatureNameList
