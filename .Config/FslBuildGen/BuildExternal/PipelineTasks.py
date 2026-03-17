#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2018 NXP
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


from FslBuildGen.BuildExternal.Tasks import CMakeAndBuildTask, DownloadTask, GitApplyTask, GitCloneTask, UnpackAndRenameTask
from FslBuildGen.Context.GeneratorContext import GeneratorContext
from FslBuildGen.Log import Log


class PipelineTasks:
    def __init__(self, log: Log, generatorContext: GeneratorContext, checkBuildCommands: bool, buildThreads: int) -> None:
        self.__Log = log
        # Add the task objects
        self.TaskCMakeAndBuild = self.__TryAllocateCMakeAndBuildTask(generatorContext, checkBuildCommands, buildThreads)
        self.TaskGitClone = self.__TryAllocateGitCloneTask(generatorContext, checkBuildCommands)
        self.TaskGitApply = self.__TryAllocateGitApplyTask(generatorContext, checkBuildCommands)
        self.TaskDownload = DownloadTask(generatorContext)
        self.TaskUnpackAndRename = UnpackAndRenameTask(generatorContext)

    def __TryAllocateCMakeAndBuildTask(self, generatorContext: GeneratorContext, checkBuildCommands: bool, buildThreads: int) -> CMakeAndBuildTask | None:
        try:
            return CMakeAndBuildTask(generatorContext, buildThreads)
        except Exception as ex:
            if checkBuildCommands:
                raise
            self.__Log.DoPrintWarning(f"CMakeAndBuild failed with: {ex}")
            return None

    def __TryAllocateGitCloneTask(self, generatorContext: GeneratorContext, checkBuildCommands: bool) -> GitCloneTask | None:
        try:
            return GitCloneTask(generatorContext)
        except Exception as ex:
            if checkBuildCommands:
                raise
            if self.__Log.Verbosity > 1:
                self.__Log.LogPrintWarning(f"GitClone is unavailable: {str(ex)}")
            return None

    def __TryAllocateGitApplyTask(self, generatorContext: GeneratorContext, checkBuildCommands: bool) -> GitApplyTask | None:
        try:
            return GitApplyTask(generatorContext)
        except Exception as ex:
            if checkBuildCommands:
                raise
            if self.__Log.Verbosity > 1:
                self.__Log.LogPrintWarning(f"GitApply is unavailable: {str(ex)}")
            return None
