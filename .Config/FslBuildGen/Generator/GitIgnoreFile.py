#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#****************************************************************************************************************************************************
#* BSD 3-Clause License
#*
#* Copyright (c) 2025, Mana Battery
#* All rights reserved.
#*
#* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
#*
#* 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
#* 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
#*    documentation and/or other materials provided with the distribution.
#* 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
#*    software without specific prior written permission.
#*
#* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
#* CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
#* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#****************************************************************************************************************************************************

from typing import List, Optional, Set
import os
import pathspec
from FslBuildGen import IOUtil

class GitDirResult(object):
    def __init__(self, ignored: Set[str], kept: Set[str]) -> None:
        self.Ignored = ignored
        self.Kept = kept



class GitIgnoreFile(object):

    @staticmethod
    def TryGetIgnoredDirectories(baseDir: str, gitignoreFile: str = ".gitignore") -> Optional[GitDirResult]:
        """
        Return GitDirResult(ignored, kept) of direct subdirectories of baseDir,
        according to patterns in gitignoreFile. Returns None if the file
        cannot be opened or parsed.
        """
        try:
            with open(gitignoreFile) as f:
                spec = pathspec.PathSpec.from_lines("gitwildmatch", f)
        except (OSError, UnicodeDecodeError, pathspec.util.RecursionError):
            return None

        ignored = set() # type: set[str]
        kept = set() # type: set[str]

        for entry in os.listdir(baseDir):
            fullPath = os.path.join(baseDir, entry)
            if os.path.isdir(fullPath):
                # Match relative to baseDir
                rel_path = os.path.relpath(fullPath, baseDir)
                # Try matching both "dir" and "dir/"
                if spec.match_file(rel_path) or spec.match_file(rel_path + "/"):
                    ignored.add(entry)
                else:
                    kept.add(entry)

        return GitDirResult(ignored, kept)


    @staticmethod
    def TryGetDirectories(filename: str) -> Optional[GitDirResult]:
        contentDir = IOUtil.GetDirectoryName(filename)
        return GitIgnoreFile.TryGetIgnoredDirectories(contentDir, filename)
