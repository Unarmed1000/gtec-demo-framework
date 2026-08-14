#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# * BSD 3-Clause License
# *
# * Copyright (c) 2026, Mana Battery
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

import xml.etree.ElementTree as ET

from FslBuildGen import IOUtil
from FslBuildGen.Log import Log
from FslBuildGen.Xml.Exceptions import XmlFormatException
from FslBuildGen.Xml.XmlBase import XmlBase
from FslBuildGen.Xml.XmlGenFileSourceGenerationGenerator import XmlGenFileSourceGenerationGenerator
from FslBuildGen.Xml.XmlGenFileSourceGenerationInputFile import XmlGenFileSourceGenerationInputFile
from FslBuildGen.Xml.XmlGenFileSourceGenerationVisibleProperty import XmlGenFileSourceGenerationVisibleProperty


class XmlGenFileSourceGeneration(XmlBase):
    """Describes that this package takes part in source generation.
    This is the concept level home for everything related to generators that run as part of the compilation.
    """

    __AttribOutputPath = "OutputPath"

    __TagGenerator = "Generator"
    __TagInputFile = "InputFile"
    __TagVisibleProperty = "VisibleProperty"
    __ValidTags = [__TagGenerator, __TagInputFile, __TagVisibleProperty]

    def __init__(self, log: Log, xmlElement: ET.Element) -> None:
        super().__init__(log, xmlElement)
        self._CheckAttributes({self.__AttribOutputPath})
        self.OutputPath: str | None = self._TryReadAttrib(xmlElement, self.__AttribOutputPath)
        if self.OutputPath is not None:
            if len(self.OutputPath) <= 0:
                raise XmlFormatException("OutputPath can not be empty")
            if IOUtil.IsAbsolutePath(self.OutputPath):
                raise XmlFormatException(f"OutputPath '{self.OutputPath}' can not be absolute")
            # The generated output directory is both a build property and a compile exclude, both of which expect a single directory name
            if "/" in self.OutputPath or "\\" in self.OutputPath:
                raise XmlFormatException(f"OutputPath '{self.OutputPath}' must be a single directory name")

        self.Generators: list[XmlGenFileSourceGenerationGenerator] = []
        self.InputFiles: list[XmlGenFileSourceGenerationInputFile] = []
        self.VisibleProperties: list[XmlGenFileSourceGenerationVisibleProperty] = []

        for child in xmlElement:
            if child.tag == self.__TagGenerator:
                self.Generators.append(XmlGenFileSourceGenerationGenerator(log, child))
            elif child.tag == self.__TagInputFile:
                self.InputFiles.append(XmlGenFileSourceGenerationInputFile(log, child))
            elif child.tag == self.__TagVisibleProperty:
                self.VisibleProperties.append(XmlGenFileSourceGenerationVisibleProperty(log, child))
            else:
                raise XmlFormatException(f"Unknown element '{child.tag}' found in SourceGeneration. Valid elements: {', '.join(self.__ValidTags)}")

        self.__CheckForDuplicates([entry.Name for entry in self.Generators], self.__TagGenerator, "Name")
        self.__CheckForDuplicates([entry.Path for entry in self.InputFiles], self.__TagInputFile, "Path")
        self.__CheckForDuplicates([entry.Name for entry in self.VisibleProperties], self.__TagVisibleProperty, "Name")

    def __CheckForDuplicates(self, values: list[str], tagName: str, attribName: str) -> None:
        uniqueValues: set[str] = set()
        for value in values:
            if value in uniqueValues:
                raise XmlFormatException(f"SourceGeneration contains a duplicated {tagName} {attribName} '{value}'")
            uniqueValues.add(value)
