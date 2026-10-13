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

# How the files of a 'Content.bld' directory are built, when the content builder of their extension with its own parameters is not it.
#
# The command file of the directory ('Content.json', version 2) can hold build rules:
#
#   "ContentBuild.Rules": [
#     { "Content": ["Shaders/*.frag"], "Builder": "VulkanShaderCompiler", "Parameters": "-V -DFAST=1 -o $(OutputFileName) $(InputFileName)" },
#     { "Content": ["flat.frag"], "Builder": "VivanteShaderCompiler", "Parameters": "-gc880 -o $(OutputFileName) $(InputFileName)",
#       "Output": "$(InputBaseName)ES2gc880.pgcSL" } ]
#
# - 'Content': the files the rule is for: files, directories and patterns with '*', as in 'Platforms' (ContentPlatformList).
# - 'Builder': the name of a content builder of the tool config. A rule whose builder is not for the app that builds the file (its features,
#   its platform) builds nothing there, as a file whose extension has no builder is not built there.
# - 'Parameters': what the builder is run with for these files, in the place of its own parameters.
# - 'Output': the name of the output below the content directory. '$(InputName)' is the path of the file below 'Content.bld',
#   '$(InputBaseName)' the same without its extension. Without it the output is named as the builder names it.
# - A file no rule matches is built by the builder of its extension, as before. A file a rule matches is built by its rules alone, once for
#   each of them: one file can give several outputs. Two rules of a file with one output name are an error.

import shlex
from collections.abc import Sequence
from typing import Any

from FslBuildGen import IOUtil
from FslBuildGen.BuildContent import ContentPlatformList
from FslBuildGen.Exceptions import UsageErrorException

RulesElement = "ContentBuild.Rules"

_g_content = "Content"
_g_builder = "Builder"
_g_parameters = "Parameters"
_g_output = "Output"
_g_inputName = "$(InputName)"
_g_inputBaseName = "$(InputBaseName)"


class ContentBuildRule:
    def __init__(self, description: str, patterns: Sequence[str], builderName: str, toolArguments: list[str] | None, output: str | None) -> None:
        """description: the rule as its writer wrote it, for the messages
        toolArguments: the parameters the builder is run with, None for its own
        output: the name of the output with its variables, None for the name the builder gives it
        """
        super().__init__()
        self.Description = description
        self.Patterns = list(patterns)
        self.BuilderName = builderName
        self.ToolArguments = toolArguments
        self.Output = output

    def IsMatch(self, relativePath: str) -> bool:
        return any(ContentPlatformList.IsPatternMatch(pattern, relativePath) for pattern in self.Patterns)

    def TryGetOutputRelativePath(self, relativePath: str) -> str | None:
        """The name of the output of a file below the content directory, None if the rule leaves the name to the builder"""
        if self.Output is None:
            return None
        baseName = relativePath[: -len(IOUtil.GetFileNameExtension(relativePath))] if len(IOUtil.GetFileNameExtension(relativePath)) > 0 else relativePath
        return self.Output.replace(_g_inputBaseName, baseName).replace(_g_inputName, relativePath)


class ContentBuildRules:
    def __init__(self, sourceFilename: str, rules: Sequence[ContentBuildRule]) -> None:
        super().__init__()
        self.SourceFilename = sourceFilename
        self.Rules = list(rules)

    def GetMatchingRules(self, relativePath: str) -> list[ContentBuildRule]:
        return [rule for rule in self.Rules if rule.IsMatch(relativePath)]

    @staticmethod
    def Parse(jsonRules: Any, sourceFilename: str) -> ContentBuildRules:
        """jsonRules: the value of 'ContentBuild.Rules' in the command file"""
        where = f"'{RulesElement}' in '{sourceFilename}'"
        if not isinstance(jsonRules, list):
            raise UsageErrorException(f"{where} is not a list of rules")
        return ContentBuildRules(sourceFilename, [ContentBuildRules.__ParseRule(jsonRule, where) for jsonRule in jsonRules])

    @staticmethod
    def __ParseRule(jsonRule: Any, where: str) -> ContentBuildRule:
        validKeys = (_g_content, _g_builder, _g_parameters, _g_output)
        if not isinstance(jsonRule, dict):
            raise UsageErrorException(f"A rule of {where} is not an object with '{_g_content}' and '{_g_builder}'")
        unknownKeys = sorted(str(key) for key in jsonRule if key not in validKeys)
        if len(unknownKeys) > 0:
            raise UsageErrorException(f"A rule of {where} has the unknown key '{unknownKeys[0]}'. Valid keys are: {', '.join(validKeys)}")
        for key in (_g_builder, _g_parameters, _g_output):
            if key in jsonRule and (not isinstance(jsonRule[key], str) or len(jsonRule[key]) <= 0):
                raise UsageErrorException(f"'{key}' of a rule of {where} is not a text")
        if _g_builder not in jsonRule:
            raise UsageErrorException(f"A rule of {where} has no '{_g_builder}': the content builder of the tool config that builds its files")
        builderName = str(jsonRule[_g_builder])
        description = f"{{{_g_builder}: {builderName}}}"
        patterns = jsonRule.get(_g_content)
        if not isinstance(patterns, list) or len(patterns) <= 0 or not all(isinstance(entry, str) and len(entry) > 0 for entry in patterns):
            raise UsageErrorException(f"'{_g_content}' of the rule {description} of {where} is not a list of at least one name")
        for pattern in patterns:
            ContentPlatformList.CheckPattern(pattern, f"the rule {description} of {where}")

        output = str(jsonRule[_g_output]) if _g_output in jsonRule else None
        if output is not None and ("\\" in output or output.startswith("/") or output.endswith("/") or ".." in output.split("/")):
            raise UsageErrorException(
                f"'{_g_output}' of the rule {description} of {where} is '{output}', which is not a path below the content directory with '/' between its parts"
            )
        toolArguments = shlex.split(str(jsonRule[_g_parameters])) if _g_parameters in jsonRule else None
        return ContentBuildRule(description, [str(entry) for entry in patterns], builderName, toolArguments, output)
