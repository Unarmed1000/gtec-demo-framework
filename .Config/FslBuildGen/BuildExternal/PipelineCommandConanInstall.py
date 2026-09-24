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

import shlex
from typing import final

from FslBuildGen.BuildExternal.PipelineCommand import PipelineCommand
from FslBuildGen.BuildExternal.PipelineInfo import PipelineInfo
from FslBuildGen.Log import Log
from FslBuildGen.PackageToolFinder import PackageToolFinder
from FslBuildGen.Xml.XmlRecipePipelineFetchCommandConanInstall import XmlRecipePipelineFetchCommandConanInstall


@final
class PipelineCommandConanInstall(PipelineCommand):
    def __init__(self, log: Log, sourceCommand: XmlRecipePipelineFetchCommandConanInstall, pipelineInfo: PipelineInfo) -> None:
        super().__init__(log, sourceCommand, pipelineInfo)
        if pipelineInfo.Tasks.TaskConanInstall is None:
            raise Exception(f"The '{sourceCommand.CommandName}' operation has not been enabled for this platform")
        self.Task = pipelineInfo.Tasks.TaskConanInstall
        self.__SourceCommand = sourceCommand

    def DoExecute(self) -> None:
        packageToolFinder = PackageToolFinder(self.Info.SourcePackage.ResolvedToolDependencyOrder)
        self.Task.RunConanInstall(
            packageToolFinder,
            self.__SourceCommand.Reference,
            self.__SourceCommand.ConfigurationList,
            shlex.split(self.__SourceCommand.Options),
            self.Info.DstRootPath,
            self.Info.AllowDownloads,
        )
