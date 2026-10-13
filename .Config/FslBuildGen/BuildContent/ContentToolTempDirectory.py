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

# Where a content tool writes before its output is taken.
#
# A content builder has its tool write to a temporary file, so the output of the app only changes when the tool made something. That file
# used to be next to the source ('Content.bld/Shader.frag.tmp'), named after it. The content of a library is built by every app that
# uses the library, each in a process of its own, and a build system runs those at the same time: they all named the same file, and one
# removed it while another still needed it. It also wrote into the directory of the library, which need not be writable.
#
# So each run of a tool gets a directory of its own, which the operating system makes with a name no other run has, outside the source
# tree and outside the content of the app. It is removed with everything in it when the run is over, however the run ends.

import contextlib
import tempfile
from collections.abc import Iterator

from FslBuildGen import IOUtil

_g_prefix = "FslBuildContent_"


@contextlib.contextmanager
def Create() -> Iterator[str]:
    """A new directory for one run of a content tool, removed when the block is left"""
    with tempfile.TemporaryDirectory(prefix=_g_prefix, ignore_cleanup_errors=True) as directory:
        yield IOUtil.NormalizePath(directory)
