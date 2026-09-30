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

import locale

from FslBuildGen import IOUtil
from FslBuildGen.Exceptions import UsageErrorException
from FslBuildGen.Log import Log


def ReadGenFileContent(log: Log, filename: str) -> str:
    """Read a gen file as text.
    Gen files declare UTF-8, so that is how they are read. A file that is not valid UTF-8 (for example one saved as ANSI) is read with the
    locale encoding, like older versions of the tool did on Windows, and a warning names the file.
    Both reads use universal newlines, so the content (and the recipe cache hash computed from it) of a pure ASCII file does not change.
    """
    try:
        return IOUtil.ReadFileUTF8(filename)
    except UnicodeDecodeError as utf8Error:
        # locale.getencoding ignores the Python UTF-8 mode, so this stays the locale encoding once UTF-8 mode is the default
        localeEncoding = locale.getencoding()
        try:
            with open(filename, encoding=localeEncoding) as file:
                content = file.read()
        except UnicodeDecodeError as localeError:
            raise UsageErrorException(
                f"The gen file '{filename}' is not valid UTF-8 (byte offset {utf8Error.start}) and it can not be read with the locale encoding "
                f"'{localeEncoding}' either. Gen files must be saved as UTF-8"
            ) from localeError
        log.DoPrintWarning(
            f"The gen file '{filename}' is not valid UTF-8 (byte offset {utf8Error.start}), it was read with the locale encoding '{localeEncoding}'. "
            "Please save it as UTF-8"
        )
        return content
