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

import os
from typing import final


@final
class ContentBuildResult:
    """What a content build did, so it can be reported and its outputs verified"""

    def __init__(self) -> None:
        super().__init__()
        # Content.bld files processed by a content processor
        self.Built = 0
        # Content files copied to the output
        self.Synced = 0
        # Files skipped as their output was up to date
        self.UpToDate = 0
        # Content.bld files no content processor handles with the active features
        self.NoProcessor = 0
        # Every output file the build is responsible for (built, copied or up to date)
        self.OutputFiles: list[str] = []

    def GetSummary(self, packageDirectory: str, contentOutputPath: str) -> str:
        summary = f"Content '{packageDirectory}': {self.Built} built, {self.Synced} copied, {self.UpToDate} up to date"
        if self.NoProcessor > 0:
            summary += f", {self.NoProcessor} without a content processor"
        return f"{summary} -> '{contentOutputPath}'"

    def GetMissingOutputFiles(self) -> list[str]:
        return [filename for filename in self.OutputFiles if not os.path.isfile(filename)]
