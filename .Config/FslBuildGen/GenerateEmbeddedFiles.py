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

# A source file that holds other files as numbers, generated from a template of the package and kept in step with what it holds.
#
#   <Generate Template="template/Shaders.cpp.txt" Target="source/Lib/Shaders.cpp">
#     <EmbedFile Name="SHADER_FRAG" File="OriginalContent/Shader.frag" Builder="VulkanShaderCompiler" Format="UInt32"/>
#   </Generate>
#
# - The template is the file to generate, with '##SHADER_FRAG##' where the numbers of that file go and '##GENERATE_STATE##' in a comment.
#   Everything else is the template's own: the names, the type of the array, the comments that switch a formatter off.
# - 'Builder' names a content builder of the tool config. The file is built by it, with the parameters it has there, and what it makes is
#   embedded: a shader is embedded as what the shader compiler gives for it, the same as an app gets it as content. Without 'Builder'
#   the bytes of the file are embedded.
# - 'Format' is how a byte is written: 'UInt8', each byte as a decimal number, or 'UInt32', four bytes as a hexadecimal word.
#
# The generated file is a file of the package like any other, meant to be in version control: whoever builds the package needs no
# builder and no tool of one. So it has to be told when it is no longer what its inputs give:
#
# - '##GENERATE_STATE##' becomes 'FslBuildGen-State: <hash>'. The hash is of the template, of every embedded file with its name and its
#   format, and of the name and the parameters of every builder. Line ends are left out of it: a checkout with other line ends is the
#   same input. The version of a tool is not part of it, so another version of a shader compiler does not make the file out of date.
# - Each time the package is resolved the hash is computed and looked for in the target. When it is there nothing is done.
# - When it is not there, the tools that make the build files (FslBuildGen, FslBuild) build the embedded files and write the target.
#   Every other tool stops: it checks or reports, and does not rewrite a file that is in version control.
# - A dry run writes and runs nothing and says what it would do.
#
# What a generated file of this kind can not have:
# - the version and git hash names of a plain 'Generate': the file would change with every commit;
# - a builder whose parameters use '$(PlatformName)': there is one file, for every platform;
# - a builder that says the file includes other files: the hash does not see a change in them.

import hashlib
import re

from FslBuildGen import IOUtil, TemplateIO
from FslBuildGen.BuildContent import ContentToolTempDirectory
from FslBuildGen.BuildContent.BasicContentProcessor import BasicContentProcessor
from FslBuildGen.BuildContent.ContentRootRecord import ContentRootRecord
from FslBuildGen.BuildContent.PathRecord import PathRecord
from FslBuildGen.BuildContent.ToolFinder import ToolFinder
from FslBuildGen.EmbedFileFormat import EmbedFileFormat
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log
from FslBuildGen.Packages.PackageEmbedFile import PackageEmbedFile
from FslBuildGen.Packages.PackageGenerate import PackageGenerate
from FslBuildGen.ToolConfig import ToolConfig, ToolContentBuilder

StateToken = "##GENERATE_STATE##"
StateText = "FslBuildGen-State: "

# The names a template of a plain 'Generate' has
ReservedNames = ("RELEASE_VERSION_MAJOR", "RELEASE_VERSION_MINOR", "RELEASE_VERSION_TWEAK", "RELEASE_VERSION_PATCH", "GIT_COMMIT_HASH", "GENERATE_STATE")

_g_stateVersion = "1"
_g_findState = re.compile(re.escape(StateText) + "([0-9a-f]{64})")
_g_platformVariable = "$(PlatformName)"
_g_valuesPerLine = {EmbedFileFormat.UInt8: 20, EmbedFileFormat.UInt32: 8}


def ToToken(name: str) -> str:
    return f"##{name}##"


def _NormalizeLineEnds(content: bytes) -> bytes:
    return content.replace(b"\r\n", b"\n")


def _ReadFile(embedFile: PackageEmbedFile, where: str) -> bytes:
    try:
        return IOUtil.ReadBinaryFile(embedFile.File.ResolvedPath)
    except OSError as ex:
        raise UsageErrorException(f"{where} embeds '{embedFile.PackageRelativePath}', which can not be read: '{embedFile.File.ResolvedPath}'") from ex


def _FindBuilders(toolConfig: ToolConfig, embedFiles: list[PackageEmbedFile], where: str) -> dict[str, ToolContentBuilder]:
    """The builders the embedded files name, by their name"""
    known = toolConfig.ContentBuilderConfiguration.ContentBuilders if toolConfig.ContentBuilderConfiguration is not None else []
    knownByName = {builder.Name: builder for builder in known}
    result: dict[str, ToolContentBuilder] = {}
    for embedFile in embedFiles:
        if embedFile.BuilderName is None:
            continue
        builder = knownByName.get(embedFile.BuilderName)
        if builder is None:
            names = ", ".join(sorted(knownByName)) if len(knownByName) > 0 else "none"
            raise UsageErrorException(
                f"{where} builds '{embedFile.PackageRelativePath}' with the builder '{embedFile.BuilderName}', which is no ContentBuilder of the "
                f"tool config. Its content builders are: {names}"
            )
        if _g_platformVariable in builder.Parameters:
            raise UsageErrorException(
                f"{where} builds '{embedFile.PackageRelativePath}' with the builder '{builder.Name}', whose parameters use {_g_platformVariable}. "
                "A generated file that is the same for every platform can not hold what is built for one"
            )
        result[builder.Name] = builder
    return result


def CheckTemplate(template: str, embedFiles: list[PackageEmbedFile], where: str) -> None:
    if StateToken not in template:
        raise UsageErrorException(
            f"{where} embeds files, so its template has to hold '{StateToken}' (in a comment): it becomes the line that tells whether the "
            "generated file is what its inputs give"
        )
    for name in ReservedNames:
        if name != "GENERATE_STATE" and ToToken(name) in template:
            raise UsageErrorException(
                f"{where} embeds files and its template uses '{ToToken(name)}'. A generated file with embedded files is kept in version "
                "control and is not to change with every version or commit"
            )
    for embedFile in embedFiles:
        if ToToken(embedFile.Name) not in template:
            raise UsageErrorException(
                f"{where} embeds '{embedFile.PackageRelativePath}' as '{embedFile.Name}', and its template has no '{ToToken(embedFile.Name)}'"
            )


def CalcState(template: str, embedFiles: list[PackageEmbedFile], builders: dict[str, ToolContentBuilder], where: str) -> str:
    """The hash of everything the generated file is made from"""
    state = hashlib.sha256()

    def Add(kind: str, content: bytes) -> None:
        # Each part with what it is and how long it is, so two parts can not be read as one
        state.update(f"{kind}:{len(content)}:".encode())
        state.update(content)

    Add("version", _g_stateVersion.encode("utf-8"))
    Add("template", _NormalizeLineEnds(template.encode("utf-8", errors="surrogateescape")))
    for embedFile in embedFiles:
        Add("name", embedFile.Name.encode("utf-8"))
        Add("path", embedFile.PackageRelativePath.encode("utf-8"))
        Add("format", embedFile.Format.value.encode("utf-8"))
        builder = builders.get(embedFile.BuilderName) if embedFile.BuilderName is not None else None
        Add("builder", b"" if builder is None else f"{builder.Name}\n{builder.Parameters}".encode())
        Add("file", _NormalizeLineEnds(_ReadFile(embedFile, where)))
    return state.hexdigest()


def TryReadState(targetFile: str) -> str | None:
    """The state the target was generated from. None for a target that is not there or holds none"""
    try:
        content = IOUtil.ReadBinaryFile(targetFile)
    except OSError:
        return None
    match = _g_findState.search(content.decode("utf-8", errors="replace"))
    return None if match is None else match.group(1)


def FormatValues(content: bytes, fileFormat: EmbedFileFormat, indent: str, description: str) -> str:
    """The bytes as the numbers of an array: separated by ', ', a fixed count on a line, the lines after the first one indented"""
    if len(content) <= 0:
        raise UsageErrorException(f"{description} is empty: there is nothing to embed")
    if fileFormat == EmbedFileFormat.UInt32:
        if (len(content) % 4) != 0:
            raise UsageErrorException(
                f"{description} is {len(content)} bytes, which is no multiple of four: it can not be embedded as {EmbedFileFormat.UInt32.value}"
            )
        values = [f"0x{int.from_bytes(content[index : index + 4], 'little'):08X}" for index in range(0, len(content), 4)]
    else:
        values = [str(value) for value in content]
    perLine = _g_valuesPerLine[fileFormat]
    lines = [", ".join(values[index : index + perLine]) for index in range(0, len(values), perLine)]
    return f",\n{indent}".join(lines)


def _BuildFile(log: Log, embedFile: PackageEmbedFile, builder: ToolContentBuilder, where: str) -> bytes:
    """What the builder makes of the file"""
    sourcePath = IOUtil.GetDirectoryName(embedFile.File.ResolvedPath)
    record = PathRecord(log, ContentRootRecord(log, sourcePath), embedFile.File.ResolvedPath)
    processor = BasicContentProcessor(log, ToolFinder(log), builder, None)
    with ContentToolTempDirectory.Create() as outputPath:
        outputFileName = IOUtil.Join(outputPath, "Output.bin")
        try:
            dependencies = processor.Process(log, False, sourcePath, outputPath, record, outputFileName=outputFileName)
        except Exception as ex:
            raise UsageErrorException(
                f"{where} is out of date and '{embedFile.PackageRelativePath}' could not be built with the builder '{builder.Name}': {ex}"
            ) from ex
        included = sorted(entry for entry in (dependencies or []) if IOUtil.NormalizePath(entry) != embedFile.File.ResolvedPath)
        if len(included) > 0:
            raise UsageErrorException(
                f"{where} embeds '{embedFile.PackageRelativePath}', which includes other files ({', '.join(included)}). That is not supported: "
                "a change in an included file would not be seen, and the generated file would stay as it is"
            )
        return IOUtil.ReadBinaryFile(outputFileName)


def _GetIndent(template: str, token: str) -> str:
    """The white space the line of the token starts with"""
    lineStart = template.rfind("\n", 0, template.index(token)) + 1
    line = template[lineStart:]
    return line[: len(line) - len(line.lstrip(" \t"))]


def Generate(log: Log, toolConfig: ToolConfig, packageName: str, entry: PackageGenerate, isDryRun: bool, allowUpdate: bool) -> bool:
    """Bring the generated file of a 'Generate' with embedded files in step with its inputs. Returns true if the file was written.
    allowUpdate: false for a tool that only checks: a file that is out of date stops it
    """
    targetFile = entry.TargetFile.ResolvedPath
    where = f"The generated file '{targetFile}' of the package '{packageName}'"
    template = TemplateIO.TryReadTemplate(log, entry.TemplateFile.ResolvedPath)
    if template is None:
        raise UsageErrorException(f"Package '{packageName}' generate template file '{entry.TemplateFile.ResolvedPath}' not found")
    CheckTemplate(template, entry.EmbedFiles, where)
    builders = _FindBuilders(toolConfig, entry.EmbedFiles, where)

    state = CalcState(template, entry.EmbedFiles, builders, where)
    if TryReadState(targetFile) == state:
        return False

    inputs = ", ".join([f"'{entry.TemplateFile.ResolvedPath}'"] + [f"'{embedFile.File.ResolvedPath}'" for embedFile in entry.EmbedFiles])
    if isDryRun:
        log.DoPrint(f"Dry run: {where} is out of date and would be generated again from {inputs}")
        return False
    if not allowUpdate:
        raise UsageErrorException(
            f"{where} is out of date: its template or a file it embeds was changed after it was generated ({inputs}). "
            "Run FslBuildGen (or FslBuild) for the package to generate it again"
        )

    content = template
    for embedFile in entry.EmbedFiles:
        builder = builders.get(embedFile.BuilderName) if embedFile.BuilderName is not None else None
        data = _ReadFile(embedFile, where) if builder is None else _BuildFile(log, embedFile, builder, where)
        description = (
            f"'{embedFile.PackageRelativePath}'" if builder is None else f"What the builder '{builder.Name}' made of '{embedFile.PackageRelativePath}'"
        )
        token = ToToken(embedFile.Name)
        content = content.replace(token, FormatValues(data, embedFile.Format, _GetIndent(template, token), f"{description} for {where[0].lower()}{where[1:]}"))
    content = content.replace(StateToken, f"{StateText}{state}")
    log.LogPrint(f"Generating '{targetFile}' ({len(entry.EmbedFiles)} embedded files)")
    IOUtil.SafeMakeDirs(IOUtil.GetDirectoryName(targetFile))
    TemplateIO.WriteGeneratedFileIfChanged(targetFile, content)
    return True
