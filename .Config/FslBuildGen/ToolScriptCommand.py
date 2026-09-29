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
import sys

from FslBuildGen import IOUtil


def GetToolScriptPath(scriptName: str) -> str:
    """The absolute path of a FslBuild entry script (like 'FslBuildContent.py'), the scripts are placed next to the FslBuildGen package"""
    toolDirectory = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    scriptPath = IOUtil.Join(toolDirectory, scriptName)
    if not IOUtil.IsFile(scriptPath):
        raise Exception(f"The FslBuild script '{scriptName}' was not found at '{scriptPath}'")
    return scriptPath


def GetToolScriptCommand(scriptName: str) -> list[str]:
    """The command that runs a FslBuild entry script with the python that runs this tool.
    Running the script directly depends on the '.py' file association and the '#!/usr/bin/env python3' shebang. On the Windows CI
    runner that went through the py launcher and silently ran nothing while returning success.
    """
    return [IOUtil.NormalizePath(sys.executable), GetToolScriptPath(scriptName)]
