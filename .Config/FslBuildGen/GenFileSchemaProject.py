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

# The schema reference a gen file of a project is expected to hold. A project is a project context of the tool config, it tells where the
# schema versions of its gen files are (ToolConfigProjectContext.GenFileSchemaLocation). A gen file belongs to the project context that
# contains its directory, the innermost one when project contexts are nested.
#
# This module reads and writes no file: a gen file that is generated gets its reference set in its text before the text is written.

from FslBuildGen import GenFileSchema, GenFileSchemaReference, IOUtil
from FslBuildGen.GenFileSchemaReference import GenFileSchemaReferenceError, ReferenceStatus
from FslBuildGen.Log import Log
from FslBuildGen.ToolConfigPackageProjectContextUtil import ToolConfigPackageProjectContextUtil
from FslBuildGen.ToolConfigProjectContext import ToolConfigProjectContext

# The name of the schema reference attribute without its prefix. A text that does not hold it has no schema reference.
_g_attributeLocalName = "noNamespaceSchemaLocation"


def TryGetExpectedReference(projectContexts: list[ToolConfigProjectContext], genFileDirectory: str, version: int = GenFileSchema.CurrentVersion) -> str | None:
    """The schema reference a gen file in the directory is expected to hold for the schema version, None when no project context contains the
    directory. genFileDirectory is the absolute path of the directory the gen file is in, written the way the tool writes a path.
    """
    projectContext = ToolConfigPackageProjectContextUtil.TryFindToProjectContext(projectContexts, genFileDirectory)
    if projectContext is None:
        return None
    return GenFileSchema.GetExpectedReference(projectContext.GenFileSchemaLocation, genFileDirectory, version)


def SetReferenceInGeneratedText(log: Log, projectContexts: list[ToolConfigProjectContext], dstFilename: str, content: str) -> str:
    """The text of a generated gen file with its schema reference set to the one that is expected for the file it is written to. The value
    of the reference is all that differs. dstFilename is the absolute path of the gen file.
    The text is returned as it is when it has no schema reference and when no project context contains the file. It is also returned as it
    is when the reference can not be set, then a warning tells why.
    """
    if _g_attributeLocalName not in content:
        return content
    expectedReference = TryGetExpectedReference(projectContexts, IOUtil.GetDirectoryName(dstFilename))
    if expectedReference is None:
        return content
    try:
        data = content.encode("utf-8")
    except UnicodeEncodeError:
        # A template that is not UTF-8 keeps its bytes undecoded in the text, that is not text the reference can be located in
        log.DoPrintWarning(f"The schema reference of the gen file '{dstFilename}' was not set. The text of the file holds bytes that are not UTF-8")
        return content
    if GenFileSchemaReference.Inspect(data).Status == ReferenceStatus.Missing:
        return content
    try:
        return GenFileSchemaReference.Replace(data, expectedReference).decode("utf-8")
    except GenFileSchemaReferenceError as ex:
        log.DoPrintWarning(f"The schema reference of the gen file '{dstFilename}' was not set. {ex}")
        return content
