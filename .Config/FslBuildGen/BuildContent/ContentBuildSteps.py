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

# What is done to build one file of a 'Content.bld' directory: which content builder is run, with what, and what the output is called.
#
# Without build rules that is one step: the content builder of the extension of the file, with its own parameters, and the output it names.
# The build rules of the command file of the directory can give a file other steps, and more than one (ContentBuildRules). The resolver
# uses the steps to know the content of an app, the content builder to build it: both ask here, so they agree.

from typing import Any

from FslBuildGen.BuildContent.ContentBuildRules import ContentBuildRules
from FslBuildGen.BuildContent.PathRecord import PathRecord
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log


class ContentBuildStep:
    def __init__(self, processor: Any, outputRelativePath: str, toolArguments: list[str] | None, builderName: str | None) -> None:
        """processor: the content builder that is run (a ContentProcessor)
        outputRelativePath: the output, below the content directory
        toolArguments: the parameters a rule gives the builder, None for its own
        builderName: the name of the builder when a rule names it, None when it is the builder of the extension of the file
        """
        super().__init__()
        self.Processor = processor
        self.OutputRelativePath = outputRelativePath
        self.ToolArguments = toolArguments
        self.BuilderName = builderName


def FindBuildSteps(log: Log, contentProcessorManager: Any, contentFile: PathRecord, rules: ContentBuildRules | None) -> list[ContentBuildStep]:
    """The steps that build the file for the app the manager is for. None means that nothing builds it there.
    contentProcessorManager: the content builders that are for the app (a ContentProcessorManager)
    rules: the build rules of the directory the file is in
    """
    matchingRules = rules.GetMatchingRules(contentFile.RelativePath) if rules is not None else []
    if len(matchingRules) <= 0:
        processor = contentProcessorManager.TryFindContentProcessor(contentFile)
        return [] if processor is None else [ContentBuildStep(processor, processor.GetOutputFileName(log, "", contentFile), None, None)]

    assert rules is not None
    steps: list[ContentBuildStep] = []
    outputs: dict[str, str] = {}
    for rule in matchingRules:
        processor = contentProcessorManager.TryFindContentProcessorByName(rule.BuilderName)
        if processor is None:
            if not contentProcessorManager.IsKnownContentProcessor(rule.BuilderName):
                raise UsageErrorException(
                    f"The rule {rule.Description} of '{rules.SourceFilename}' names a content builder the tool config does not have. "
                    f"The content builders are: {', '.join(contentProcessorManager.GetKnownContentProcessorNames())}"
                )
            # The builder is not for this app (its features, its platform): the rule builds nothing here
            continue
        outputRelativePath = rule.TryGetOutputRelativePath(contentFile.RelativePath)
        if outputRelativePath is None:
            outputRelativePath = processor.GetOutputFileName(log, "", contentFile)
        if outputRelativePath in outputs:
            raise UsageErrorException(
                f"The rules {outputs[outputRelativePath]} and {rule.Description} of '{rules.SourceFilename}' both build '{contentFile.RelativePath}' "
                f"into '{outputRelativePath}'. Give one of them an 'Output' of its own"
            )
        outputs[outputRelativePath] = rule.Description
        steps.append(ContentBuildStep(processor, outputRelativePath, rule.ToolArguments, rule.BuilderName))
    return steps
