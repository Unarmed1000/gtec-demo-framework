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

# The names in gen files that the tool does not read (Xml/XmlNameCheck.py): every one of a load of packages.
#
# A gen file, or a template a gen file imports, that holds an attribute or an element its readers do not read stops the load: one error
# for the first file that has one. A project that meets this for the first time wants the files it has to fix, all of them at once. This
# loads the packages without stopping at such a file and gives the names of every file that was read.

from collections.abc import Sequence

from FslBuildGen.Config import Config
from FslBuildGen.Generator.GeneratorPluginBase import GeneratorPluginBase
from FslBuildGen.PackageLoader import PackageLoader
from FslBuildGen.Xml import XmlNameCheck
from FslBuildGen.Xml.XmlNameCheck import XmlUnknownName


def FindUnknownNames(config: Config, genFiles: Sequence[str], platformGeneratorPlugin: GeneratorPluginBase) -> list[XmlUnknownName]:
    """Load the gen files, and the packages they depend on, the way every tool does, without stopping at a file that holds an unknown
    name. Returns the unknown names of every gen file and template that was read: by file, in the order they are in a file.
    Any other error of a gen file stops the load, as it does in a tool.
    """
    with XmlNameCheck.CollectUnknownNames() as collector:
        PackageLoader(config, list(genFiles), platformGeneratorPlugin)
    byFile: dict[str, list[XmlUnknownName]] = {}
    for entry in collector.UnknownNames:
        byFile.setdefault(entry.FileName, []).append(entry)
    return [entry for fileName in sorted(byFile) for entry in byFile[fileName]]
