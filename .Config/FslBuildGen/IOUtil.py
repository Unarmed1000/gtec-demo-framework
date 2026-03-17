#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright (c) 2014 Freescale Semiconductor, Inc.
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
#    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
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

import contextlib
import errno
import hashlib
import os
import os.path
import shutil
import stat
import sys
from typing import Any

# from FslBuildGen.Exceptions import *
# from FslBuildGen import Util


def ReadFile(filename: str, newline: str | None = None) -> str:
    content = None
    with open(filename, newline=newline) as theFile:
        content = str(theFile.read())
    return content


def TryReadFile(filename: str) -> str | None:
    try:
        return ReadFile(filename)
    except OSError:
        return None


def ReadFileUTF8(filename: str, newline: str | None = None) -> str:
    content = None
    with open(filename, newline=newline, encoding="utf-8") as theFile:
        content = str(theFile.read())
    return content


def WriteFileUTF8(filename: str, content: str, newline: str | None = None) -> None:
    with open(filename, "w", newline=newline, encoding="utf-8") as theFile:
        theFile.write(content)


def WriteFileUTF8IfChanged(filename: str, content: str, newline: str | None = None) -> bool:
    existingContent = None
    if os.path.exists(filename):
        if os.path.isfile(filename):
            existingContent = ReadFileUTF8(filename)
        else:
            raise OSError(f"'{filename}' exist but it's not a file")

    if content == existingContent:
        return False
    WriteFileUTF8(filename, content, newline=newline)
    return True


def WriteFile(filename: str, content: str, newline: str | None = None) -> None:
    with open(filename, "w", newline=newline) as theFile:
        theFile.write(content)


def WriteFileIfChanged(filename: str, content: str, newline: str | None = None) -> bool:
    existingContent = None
    if os.path.exists(filename):
        if os.path.isfile(filename):
            existingContent = ReadFile(filename)
        else:
            raise OSError(f"'{filename}' exist but it's not a file")

    if content == existingContent:
        return False
    WriteFile(filename, content, newline=newline)
    return True


def ReadBinaryFile(filename: str) -> bytes:
    content = None
    with open(filename, "rb") as theFile:
        content = theFile.read()
    return content


def TryReadBinaryFile(filename: str) -> bytes | None:
    try:
        return ReadBinaryFile(filename)
    except OSError:
        return None


def WriteBinaryFile(filename: str, content: bytes) -> None:
    with open(filename, "wb") as theFile:
        theFile.write(content)


def WriteBinaryFileIfChanged(filename: str, content: bytes) -> None:
    existingContent = None
    if os.path.exists(filename):
        if os.path.isfile(filename):
            existingContent = ReadBinaryFile(filename)
        else:
            raise OSError(f"'{filename}' exist but it's not a file")
    if content != existingContent:
        WriteBinaryFile(filename, content)


def FileLength(filename: str) -> int:
    return os.stat(filename).st_size


def SetFileExecutable(filename: str) -> None:
    st = os.stat(filename)
    os.chmod(filename, st.st_mode | stat.S_IEXEC)
    st = os.stat(filename)


def Exists(path: str) -> bool:
    return os.path.exists(path)


def IsFile(filename: str) -> bool:
    return os.path.isfile(filename)


def IsDirectory(path: str) -> bool:
    return os.path.isdir(path)


def RemoveFile(filename: str) -> None:
    with contextlib.suppress(OSError):
        os.remove(filename)


def IsAbsolutePath(sourcePath: str) -> bool:
    return os.path.isabs(sourcePath)


def RemoveAllContent(pathDir: str, directoryMustExist: bool = True) -> None:
    """Removes all files and directories from the given directory"""
    if not IsDirectory(pathDir):
        if directoryMustExist:
            raise Exception(f"Usage error '{pathDir}' is not a directory")
        return

    for item in os.listdir(pathDir):
        fullPath = os.path.join(pathDir, item)
        if IsFile(fullPath):
            os.remove(fullPath)
        elif IsDirectory(fullPath):
            shutil.rmtree(fullPath)


def GetEnvironmentVariables() -> dict[str, str]:
    return dict(os.environ.items())


def TryGetEnvironmentVariable(name: str) -> str | None:
    return os.environ.get(name)


def GetEnvironmentVariable(name: str) -> str:
    result = os.environ.get(name)
    if result is None:
        raise OSError(f"'{name}' environment variable not set")
    return result


def GetEnvironmentVariableForAbsolutePath(name: str) -> str:
    path = TryGetEnvironmentVariable(name)
    if path is None:
        raise OSError(f"'{name}' environment variable not set")
    path = NormalizePath(path)
    if path is None:
        raise OSError(f"'{name}' environment variable not set")
    if not os.path.isabs(path):
        raise OSError(f"'{name}' environment path '{path}' is not absolute")
    if path.endswith("/"):
        raise OSError(f"'{name}' environment path '{path}' not allowed to end with '/' or ''")
    return path


def GetEnvironmentVariableForDirectory(name: str, mustExist: bool = True) -> str:
    path = TryGetEnvironmentVariable(name)
    if path is None:
        raise OSError(f"{name} environment variable not set")
    path = NormalizePath(path)
    if path is None:
        raise OSError(f"{name} environment variable not set")
    if not os.path.isabs(path):
        raise OSError(f"{name} environment path '{path}' is not absolute")
    if path.endswith("/"):
        raise OSError(f"{name} environment path '{path}' not allowed to end with '/' or ''")
    if mustExist and not os.path.isdir(path):
        raise OSError(f"The {name} environment variable content '{path}' does not point to a valid directory")
    return path


def SafeMakeDirs(path: str) -> None:
    try:
        os.makedirs(path)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(path):
            pass
        else:
            raise


# TODO: find the correct type for the excinfo
def __OnRMError(func: object, path: str, excinfo: Any) -> None:
    # Deal with read only files
    if IsFile(path):
        os.chmod(path, stat.S_IWRITE)
        os.remove(path)
    elif IsDirectory(path):
        os.chmod(path, stat.S_IWRITE)
        os.removedirs(path)
    else:
        raise excinfo[0](excinfo[1]).with_traceback(excinfo[2])


def SafeRemoveDirectoryTree(path: str, logExceptionAsWarningButContinue: bool = False) -> None:
    """Beware this also removes read only files"""
    try:
        if IsDirectory(path):
            shutil.rmtree(path, onerror=__OnRMError)
    except Exception as ex:
        if not logExceptionAsWarningButContinue:
            raise
        print(f"WARNING: Could not remove the directory at '{path}' because {str(ex)}")


def CopySmallFile(srcFilename: str, dstFilename: str) -> None:
    srcContent = None
    dstContent = None
    if os.path.exists(srcFilename):
        if os.path.isfile(srcFilename):
            srcContent = ReadBinaryFile(srcFilename)
        else:
            raise OSError(f"'{srcFilename}' exist but it's not a file")

    if os.path.exists(dstFilename):
        if os.path.isfile(dstFilename):
            dstContent = ReadBinaryFile(dstFilename)
        else:
            raise OSError(f"'{dstFilename}' exist but it's not a file")

    if srcContent is None:
        raise OSError(f"'{srcFilename}' not found")

    if srcContent != dstContent:
        if dstContent is None:
            dstDirName = os.path.dirname(dstFilename)
            if not os.path.exists(dstDirName):
                SafeMakeDirs(dstDirName)
        WriteBinaryFileIfChanged(dstFilename, srcContent)


def ToUnixStylePath(path: str) -> str:
    # Workaround the fact that paths on windows sometimes come with a uppercase drive letter and sometimes a lowercase
    if len(path) > 2 and (path[1] == ":" and (path[0] >= "a" and path[0] <= "z")):
        path = path[0].upper() + path[1:]
    return path.replace("\\", "/")


def TryToUnixStylePath(path: str | None) -> str | None:
    if path is None:
        return None
    return path.replace("\\", "/")


def NormalizePath(path: str) -> str:
    return ToUnixStylePath(os.path.normpath(path))


def RelativePath(path: str, start: str) -> str:
    return NormalizePath(os.path.relpath(path, start))


def Join(path1: str, path2: str) -> str:
    return ToUnixStylePath(os.path.join(path1, path2))


def GetFileName(path: str) -> str:
    return os.path.basename(path)


def GetFileNameWithoutExtension(path: str) -> str:
    return os.path.splitext(os.path.basename(path))[0]


def GetFileNameExtension(path: str) -> str:
    return os.path.splitext(os.path.basename(path))[1]


def __IgnoreFile(ignoreDirectories: list[str], filename: str) -> bool:
    return any(filename.startswith(dirpath) for dirpath in ignoreDirectories)


def FindFileByName(directory: str, findFilename: str, ignoreDirectories: list[str] | None = None) -> list[str]:
    """
    This function will find all instances of a findFilename in the directory and its subdirectories
    :param ignoreDirectories: Will not scan any of the ignored directories.
    """
    filePaths: list[str] = []  # List which will store all of the full filepaths.

    try:
        if ignoreDirectories is None or directory not in ignoreDirectories:
            # Walk the tree.
            for root, directories, files in os.walk(directory):
                if ignoreDirectories is not None:
                    directories[:] = [dir for dir in directories if ToUnixStylePath(os.path.join(root, dir)) not in ignoreDirectories]
                for filename in files:
                    if filename == findFilename:
                        # Join the two strings in order to form the full filepath.
                        filepath = ToUnixStylePath(os.path.join(root, filename))
                        filePaths.append(filepath)  # Add it to the list.
    except StopIteration:  # Python >2.5
        pass
    return filePaths


def ContainsFileByName(directory: str, findFilename: str, ignoreDirectories: list[str] | None = None) -> str | None:
    """
    This function will find all instances of a findFilename in the directory and its subdirectories
    :param ignoreDirectories: Will not scan any of the ignored directories.
    """

    try:
        if ignoreDirectories is None or directory not in ignoreDirectories:
            # Walk the tree.
            for root, directories, files in os.walk(directory):
                if ignoreDirectories is not None:
                    directories[:] = [dir for dir in directories if ToUnixStylePath(os.path.join(root, dir)) not in ignoreDirectories]
                for filename in files:
                    if filename == findFilename:
                        # Join the two strings in order to form the full filepath.
                        filepath = ToUnixStylePath(os.path.join(root, filename))
                        return filepath  # Add it to the list.
    except StopIteration:  # Python >2.5
        pass
    return None


def FindFileByExtension(directory: str, extension: str, ignoreDirectories: list[str] | None = None) -> list[str]:
    """
    This function will find all instances of files with the given extension in the directory and its subdirectories
    :param ignoreDirectories: Will not scan any of the ignored directories.
    """
    filePaths: list[str] = []  # List which will store all of the full filepaths.

    try:
        if ignoreDirectories is None or directory not in ignoreDirectories:
            # Walk the tree.
            for root, directories, files in os.walk(directory):
                if ignoreDirectories is not None:
                    directories[:] = [dir for dir in directories if ToUnixStylePath(os.path.join(root, dir)) not in ignoreDirectories]
                for filename in files:
                    if filename.endswith(extension):
                        # Join the two strings in order to form the full filepath.
                        filepath = ToUnixStylePath(os.path.join(root, filename))
                        filePaths.append(filepath)  # Add it to the list.
    except StopIteration:  # Python >2.5
        pass
    return filePaths


def GetFilePaths(directory: str, endswithFilter: str | tuple[str, ...] | None) -> list[str]:
    """
    This function will generate the file names in a directory
    tree by walking the tree either top-down or bottom-up. For each
    directory in the tree rooted at directory top (including top itself),
    it yields a 3-tuple (dirpath, dirnames, filenames).
    """
    filePaths: list[str] = []  # List which will store all of the full filepaths.

    try:
        # Walk the tree.
        for root, _directories, files in os.walk(directory):
            for filename in files:
                if endswithFilter is None or filename.endswith(endswithFilter):
                    # Join the two strings in order to form the full filepath.
                    filepath = os.path.join(root, filename)
                    filePaths.append(NormalizePath(filepath))  # Add it to the list.
    except StopIteration:  # Python >2.5
        pass
    return filePaths


def GetFilesAt(directory: str, absolutePaths: bool) -> list[str]:
    if absolutePaths:
        return [Join(directory, f) for f in os.listdir(directory) if os.path.isfile(os.path.join(directory, f))]
    return [f for f in os.listdir(directory) if os.path.isfile(os.path.join(directory, f))]


def GetDirectoriesAt(directory: str, absolutePaths: bool) -> list[str]:
    """
    This function will generate the file names in a directory
    tree by walking the tree either top-down or bottom-up. For each
    directory in the tree rooted at directory top (including top itself),
    it yields a 3-tuple (dirpath, dirnames, filenames).
    """

    # file_paths: List[str] = []  # List which will store all of the full filepaths.

    # Walk the tree.
    res: list[str] = []
    try:
        root, directories, files = next(os.walk(directory))

        for path in directories:
            dirpath = path
            if absolutePaths:
                dirpath = os.path.join(root, path)
            res.append(NormalizePath(dirpath))
    except StopIteration:  # Python >2.5
        pass
    return res


def GetExecutablePath() -> str:
    return NormalizePath(os.path.dirname(sys.argv[0]))


def GetDirectoryName(path: str) -> str:
    directoryName = NormalizePath(os.path.dirname(path))
    return directoryName if directoryName != "." else ""


def TryFindFileInCurrentOrParentDir(path: str, filename: str) -> str | None:
    oldPath = None
    while path != oldPath:
        fullPath = os.path.join(path, filename)
        if os.path.isfile(fullPath):
            return NormalizePath(fullPath)
        oldPath = path
        path = os.path.dirname(path)
    return None


def TryFindExecutable(program: str) -> str | None:
    """Try to locate the given executable"""

    def IsExe(fpath: str) -> bool:
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if IsExe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exeFile = os.path.join(path, program)
            if IsExe(exeFile):
                return exeFile
    return None


def TryFindFileInPath(filename: str) -> str | None:
    fpath, fname = os.path.split(filename)
    if fpath:
        if os.path.isfile(filename):
            return filename
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exeFile = os.path.join(path, filename)
            if os.path.isfile(exeFile):
                return exeFile
    return None


def GetCurrentWorkingDirectory() -> str:
    return NormalizePath(os.getcwd())


def HashFile(filename: str, blocksize: int = 65536) -> str:
    hasher = hashlib.sha256()
    with open(filename, "rb") as theFile:
        buf = theFile.read(blocksize)
        while len(buf) > 0:
            hasher.update(buf)
            buf = theFile.read(blocksize)
        return hasher.hexdigest()


def IsDriveRootPath(path: str) -> bool:
    """
    Do some basic checks to prevent a path that points to the drive root
    - This also detects some valid names like "test../" or "/..test"
    """
    normPath = NormalizePath(path)
    drive, tail = os.path.splitdrive(normPath)
    driveId = NormalizePath(drive).lower()
    normPathId = normPath.lower()
    # some basic checks to detect root paths
    return (
        "../" in normPath
        or "/.." in normPath
        or normPath == "/"
        or normPath == ".."
        or normPath == "/."
        or len(normPath) <= 0
        or normPathId == driveId
        or normPath.endswith("/")
    )
