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

# What an element of a xml file holds beyond what its reader reads: attributes and child elements with a name the reader does not know.
#
# A reader picks the children and attributes it knows by name and never looks at the rest, so a name with a typing error, or an element
# in a place it is not read in, has no effect and nobody is told. Each reader says which names it reads (the same place that reads them:
# XmlBase._CheckAttributes, or CheckNames for an element that has no reader class) and the names of the element are looked up in those.
#
# - Names only: not the order, not the count and not the values.
# - Every element a reader constructs is checked, whatever the platform of the run is: a reader reads the elements of every platform.
# - A comment or a processing instruction is no element.
#
# The names are collected for the file that is being read: the reader of a file reads it inside 'with ReadFile(fileName)'. When the
# file is read, its unknown names stop the load with one error that lists them (XmlUnknownNamesException), so a file is fixed once.
# A file that is read while another one is being read (a template a gen file imports) has its own list. Without a file that is being
# read nothing is looked up.
#
# An element whose reader reads more names than the place of the element can hold (a package element: what it can hold depends on the
# type of the package) says so with a XmlNamePlace. A name the reader knows that the place can not hold is one of the names of the file
# too: it is reported as not valid in that place, not as unknown.
#
# A tool that lists the unknown names of a whole project does not want to stop at the first file: inside
# 'with CollectUnknownNames() as collector' nothing is raised and the collector gets the names of every file that is read.
#
# The cost for a file without unknown names is one set lookup per attribute and child element of the elements the readers visit anyway.
# The path of an element is only worked out for a file that has an unknown name.

import xml.etree.ElementTree as ET
from collections.abc import Collection, Sequence
from types import TracebackType

from FslBuildGen.MatchUtil import MatchUtil
from FslBuildGen.Xml.Exceptions import XmlException2

# The attributes any root element can have: the reference to its schema ('xsi:noNamespaceSchemaLocation')
_g_schemaInstancePrefix = "{http://www.w3.org/2001/XMLSchema-instance}"

NoNames: frozenset[str] = frozenset()

# The attribute that tells an element from the others of its tag in a path
_g_nameAttribute = "Name"
_g_noElements: frozenset[ET.Element] = frozenset()


class XmlNamePlace:
    """What an element is, for an element whose reader reads names its place can not hold: 'an ExternalLibrary', and the attributes
    and the child elements the reader reads in another place.
    """

    def __init__(self, name: str, otherAttributes: Collection[str], otherElements: Collection[str]) -> None:
        super().__init__()
        # What the element is, with its article: it ends the sentence '... is not valid in'
        self.Name = name
        self.OtherAttributes = frozenset(otherAttributes)
        self.OtherElements = frozenset(otherElements)


class XmlUnknownName:
    """An attribute or a child element of an element that the reader of the element does not read"""

    def __init__(self, fileName: str, elementPath: str, isAttribute: bool, name: str, validNames: Collection[str], place: str | None = None) -> None:
        super().__init__()
        self.FileName = fileName
        # Where the element is: the names of the elements from the root to it, see GetElementPath
        self.ElementPath = elementPath
        # True: an attribute of the element. False: a child element of it.
        self.IsAttribute = isAttribute
        self.Name = name
        # The names the reader reads there, sorted
        self.ValidNames = sorted(validNames)
        # The valid name that is most like the name, empty when there is no valid name
        self.ClosestMatch = MatchUtil.BuildCandidateListString(name, self.ValidNames, 1)
        # None for a name no reader of the element reads. For a name the reader reads in another place: what the element is, 'an
        # ExternalLibrary' (XmlNamePlace). The name is right, it is the place that can not hold it.
        self.Place = place

    def __str__(self) -> str:
        return self.Describe(True)

    def Describe(self, withFile: bool) -> str:
        """What is unknown, where, the closest valid name and the valid names. The file is named unless the text is one of a list for
        one file.
        """
        kind = "attribute" if self.IsAttribute else "element"
        where = f"'{self.ElementPath}' of '{self.FileName}'" if withFile else f"'{self.ElementPath}'"
        if self.Place is not None:
            valid = f"Valid {kind}s are: {', '.join(self.ValidNames)}" if len(self.ValidNames) > 0 else f"It can not contain {kind}s"
            return f"{kind.capitalize()} '{self.Name}' in {where} is not valid in {self.Place}. {valid}"
        if len(self.ValidNames) <= 0:
            return f"Unknown {kind} '{self.Name}' in {where}: this element can not contain {kind}s"
        return f"Unknown {kind} '{self.Name}' in {where}, did you mean '{self.ClosestMatch}'. Valid {kind}s are: {', '.join(self.ValidNames)}"


class XmlUnknownNamesException(XmlException2):
    """A file holds attributes or elements that its readers do not read. One error for the file, it lists them all (the first
    MaxListed of them) so the file can be fixed in one go.
    """

    MaxListed = 10

    def __init__(self, fileName: str, unknownNames: Sequence[XmlUnknownName]) -> None:
        if len(unknownNames) == 1:
            message = str(unknownNames[0])
        else:
            lines = [f"'{fileName}' holds {len(unknownNames)} names that are not known:"]
            lines += [f"- {entry.Describe(False)}" for entry in unknownNames[: self.MaxListed]]
            if len(unknownNames) > self.MaxListed:
                lines.append(f"- and {len(unknownNames) - self.MaxListed} more")
            message = "\n".join(lines)
        super().__init__(message)
        self.FileName = fileName
        self.UnknownNames = list(unknownNames)


def _DescribeElement(element: ET.Element, parent: ET.Element | None, withoutName: Collection[ET.Element]) -> str:
    """The tag of the element, with what tells it from the other elements of that tag in its parent: its name, or else its position"""
    tag = str(element.tag)
    if parent is None:
        return tag
    name = None if element in withoutName else element.attrib.get(_g_nameAttribute)
    if name is not None:
        return f"{tag}[@Name='{name}']"
    sameTag = [child for child in parent if child.tag == element.tag]
    return tag if len(sameTag) <= 1 else f"{tag}[{sameTag.index(element) + 1}]"


def GetElementPath(root: ET.Element, element: ET.Element, withoutName: Collection[ET.Element] = _g_noElements) -> str:
    """The path of an element of the tree, 'FslBuildGen/Executable[@Name='App']/Platform[@Name='Windows']/Variant[2]'. An element that
    is not part of the tree is named by its tag. withoutName are the elements whose 'Name' attribute is not a name of theirs (their
    reader does not read it): they are told by their position.
    """
    parents: dict[ET.Element, ET.Element] = {child: parent for parent in root.iter() for child in parent}
    parts: list[str] = []
    current: ET.Element | None = element
    while current is not None:
        parent = parents.get(current)
        parts.append(_DescribeElement(current, parent, withoutName))
        current = parent
    return "/".join(reversed(parts))


class XmlNameCheckFile:
    """The unknown names of one file that is being read"""

    def __init__(self, fileName: str) -> None:
        super().__init__()
        self.FileName = fileName
        self.Root: ET.Element | None = None
        # (the element, True for an attribute, the name, the names the reader reads, the place for a name that is known elsewhere)
        self.__Found: list[tuple[ET.Element, bool, str, Collection[str], str | None]] = []

    def Add(self, element: ET.Element, isAttribute: bool, name: str, validNames: Collection[str], place: str | None = None) -> None:
        # An element can be read more than once (a platform element that names several platforms)
        for entry in self.__Found:
            if entry[0] is element and entry[1] == isAttribute and entry[2] == name:
                return
        self.__Found.append((element, isAttribute, name, validNames, place))

    def GetUnknownNames(self) -> list[XmlUnknownName]:
        """In the order they were found"""
        if len(self.__Found) <= 0:
            return []
        root = self.Root
        # An element whose 'Name' is one of the unknown attributes is not told by it: the path would hold the name that is not known
        withoutName = {element for element, isAttribute, name, _, place in self.__Found if isAttribute and name == _g_nameAttribute and place is None}
        return [
            XmlUnknownName(
                self.FileName, str(element.tag) if root is None else GetElementPath(root, element, withoutName), isAttribute, name, validNames, place
            )
            for element, isAttribute, name, validNames, place in self.__Found
        ]


# The files that are being read, the last one is the one the names are collected for
_g_files: list[XmlNameCheckFile] = []


def BeginFile(fileName: str) -> XmlNameCheckFile:
    """Start to collect the unknown names of a file. Every BeginFile has its EndFile, also when reading the file fails."""
    file = XmlNameCheckFile(fileName)
    _g_files.append(file)
    return file


def EndFile(file: XmlNameCheckFile) -> list[XmlUnknownName]:
    """Stop collecting for the file and return what was found"""
    if len(_g_files) <= 0 or _g_files[-1] is not file:
        raise Exception(f"EndFile of '{file.FileName}' does not match the file that is being read")
    _g_files.pop()
    return file.GetUnknownNames()


def IsActive() -> bool:
    return len(_g_files) > 0


class XmlUnknownNameCollector:
    """What a tool that lists the unknown names of many files gets instead of an error at the first file that has one"""

    def __init__(self) -> None:
        super().__init__()
        # The unknown names of every file that was read, in the order the files were read
        self.UnknownNames: list[XmlUnknownName] = []
        # Every file that was read, also the ones without an unknown name
        self.FilesRead: list[str] = []


# The collector of the tool that lists the names, the last one is the one that is used. Empty: unknown names stop the load.
_g_collectors: list[XmlUnknownNameCollector] = []


class CollectUnknownNames:
    """with CollectUnknownNames() as collector: a file with unknown names does not stop the load, the collector gets them"""

    def __init__(self) -> None:
        super().__init__()
        self.__Collector = XmlUnknownNameCollector()

    def __enter__(self) -> XmlUnknownNameCollector:
        _g_collectors.append(self.__Collector)
        return self.__Collector

    def __exit__(self, excType: type[BaseException] | None, exc: BaseException | None, traceback: TracebackType | None) -> None:
        _g_collectors.remove(self.__Collector)


class ReadFile:
    """with ReadFile(fileName): read the file with its readers. At the end the unknown names of the file stop the load, unless the
    reading itself stopped with an error (that error is the one that is reported) or the names are being collected.
    """

    def __init__(self, fileName: str) -> None:
        super().__init__()
        self.__File = XmlNameCheckFile(fileName)

    def __enter__(self) -> XmlNameCheckFile:
        _g_files.append(self.__File)
        return self.__File

    def __exit__(self, excType: type[BaseException] | None, exc: BaseException | None, traceback: TracebackType | None) -> None:
        unknownNames = EndFile(self.__File)
        if len(_g_collectors) > 0:
            _g_collectors[-1].FilesRead.append(self.__File.FileName)
            _g_collectors[-1].UnknownNames += unknownNames
        elif excType is None and len(unknownNames) > 0:
            raise XmlUnknownNamesException(self.__File.FileName, unknownNames)


def StopAtUnknownNames() -> None:
    """For a reader that is about to stop because the file lacks an element it needs: when the file that is being read holds unknown
    names, they are the error that is reported (the element may be there with a typing error). Nothing happens when the names are
    collected, or when the file holds none: the reader then stops with what it has to say.
    """
    if len(_g_files) > 0 and len(_g_collectors) <= 0:
        unknownNames = _g_files[-1].GetUnknownNames()
        if len(unknownNames) > 0:
            raise XmlUnknownNamesException(_g_files[-1].FileName, unknownNames)


def CheckElements(element: ET.Element, validElements: Collection[str], place: XmlNamePlace | None = None) -> None:
    """Collect the child elements of the element whose name is not one of validElements. With a place: one of them that the reader of
    the element reads in another place is collected as not valid in this one.
    """
    if len(_g_files) > 0 and len(element) > 0:
        for child in element:
            if child.tag not in validElements and isinstance(child.tag, str):
                _g_files[-1].Add(element, False, child.tag, validElements, place.Name if place is not None and child.tag in place.OtherElements else None)


def TryAddAttributeOfAnotherPlace(element: ET.Element, name: str, validAttributes: Collection[str], place: XmlNamePlace) -> bool:
    """For a reader that found an attribute that is not one of its place: when the reader reads the attribute in another place and a
    file is being read, the attribute is collected as not valid in this place and True is returned. Otherwise the reader stops at it.
    """
    if len(_g_files) <= 0 or name not in place.OtherAttributes:
        return False
    _g_files[-1].Add(element, True, name, validAttributes, place.Name)
    return True


def CheckAttributes(element: ET.Element, validAttributes: Collection[str]) -> None:
    """Collect the attributes of the element whose name is not one of validAttributes. For an element whose reader does not stop at an
    unknown attribute itself (XmlBase._CheckAttributes does, with the error it always had).
    """
    if len(_g_files) > 0 and len(element.attrib) > 0:
        for name in element.attrib:
            if name not in validAttributes:
                _g_files[-1].Add(element, True, name, validAttributes)


def CheckNames(element: ET.Element, validAttributes: Collection[str], validElements: Collection[str]) -> None:
    CheckAttributes(element, validAttributes)
    CheckElements(element, validElements)


def CheckRoot(root: ET.Element, validAttributes: Collection[str], validElements: Collection[str]) -> None:
    """The root element of the file that is being read: it is what the paths of the file start at. A root can refer to its schema."""
    if len(_g_files) > 0:
        file = _g_files[-1]
        file.Root = root
        for name in root.attrib:
            if name not in validAttributes and not name.startswith(_g_schemaInstancePrefix):
                file.Add(root, True, name, validAttributes)
        CheckElements(root, validElements)
