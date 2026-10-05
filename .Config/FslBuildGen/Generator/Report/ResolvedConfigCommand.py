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

# The configure command of a build as it is run: the format strings of the generator's config report with the variables filled in.
#
# The builder runs this command (Builder.__ConfigureBuild) and the CMake presets of a generated project are made from it
# (Generator/CMakePresetsFile.py). Both get it here, so the two can not resolve it differently.

from FslBuildGen.ExternalVariantConstraints import ExternalVariantConstraints
from FslBuildGen.Generator.Report.GeneratorConfigCommandReport import GeneratorConfigCommandReport
from FslBuildGen.Generator.Report.GeneratorVariableReport import GeneratorVariableReport
from FslBuildGen.Generator.Report.ReportVariableFormatter import ReportVariableFormatter


class ResolvedConfigCommand:
    def __init__(self, command: list[str], currentWorkingDirectory: str) -> None:
        super().__init__()
        # The program followed by its arguments
        self.Command = command
        # The directory the command is run in
        self.CurrentWorkingDirectory = currentWorkingDirectory


def Resolve(
    configCommandReport: GeneratorConfigCommandReport, variableReport: GeneratorVariableReport, variantConstraints: ExternalVariantConstraints
) -> ResolvedConfigCommand:
    """Fill in the variables of the configure command for the variant options that were chosen (a variable nothing was chosen for gets its
    default option). The variable report is the one of the config report with the custom variables added (BuildUtil.AddCustomVariables).
    """
    arguments = [ReportVariableFormatter.Format(argument, variableReport, variantConstraints) for argument in configCommandReport.Arguments]
    command = ReportVariableFormatter.Format(configCommandReport.CommandFormatString, variableReport, variantConstraints)
    currentWorkingDirectory = ReportVariableFormatter.Format(configCommandReport.CurrentWorkingDirectoryFormatString, variableReport, variantConstraints)
    return ResolvedConfigCommand([command, *arguments], currentWorkingDirectory)
