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
import subprocess
from collections.abc import Sequence
from typing import NamedTuple

from FslBuildGen.GitUtil import GitUtil
from FslBuildGen.PlatformUtil import PlatformUtil

# The environment variables that make git use another repository, work tree, index or object store than the one found from the working
# directory, or that change how the paths we pass are interpreted. They are removed from the environment of the git process.
# The GIT_CONFIG_* variables are kept, a CI system can use them to configure 'safe.directory'.
g_removedEnvironmentVariables: tuple[str, ...] = (
    "GIT_DIR",
    "GIT_WORK_TREE",
    "GIT_COMMON_DIR",
    "GIT_INDEX_FILE",
    "GIT_OBJECT_DIRECTORY",
    "GIT_ALTERNATE_OBJECT_DIRECTORIES",
    "GIT_IMPLICIT_WORK_TREE",
    "GIT_PREFIX",
    "GIT_LITERAL_PATHSPECS",
    "GIT_GLOB_PATHSPECS",
    "GIT_NOGLOB_PATHSPECS",
    "GIT_ICASE_PATHSPECS",
)


class GitRunResult(NamedTuple):
    ExitCode: int
    Stdout: bytes
    Stderr: str


class GitRunError(Exception):
    """Git could not be run at all, a git process that ran and failed is reported through GitRunResult.ExitCode instead"""

    def __init__(self, message: str, executableNotFound: bool) -> None:
        super().__init__(message)
        self.ExecutableNotFound = executableNotFound


def CreateGitEnvironment(environment: dict[str, str]) -> dict[str, str]:
    """Return a copy of 'environment' for a git process that must only use the repository found from its working directory and never prompts"""
    result = dict(environment)
    # Environment variable names are case insensitive on Windows
    caseInsensitive = os.name == "nt"
    removeSet = set(g_removedEnvironmentVariables)
    for key in list(result.keys()):
        if (key.upper() if caseInsensitive else key) in removeSet:
            del result[key]
    result["GIT_TERMINAL_PROMPT"] = "0"
    return result


class GitRunner:
    def __init__(self, gitExecutable: str, timeoutSeconds: float = 60) -> None:
        super().__init__()
        self.GitExecutable = gitExecutable
        self.TimeoutSeconds = timeoutSeconds

    @staticmethod
    def CreateForHost() -> GitRunner:
        return GitRunner(GitUtil.GetPlatformDependentExecutableName(PlatformUtil.DetectBuildPlatformType()))

    def Run(self, workingDirectory: str, args: Sequence[str], stdinData: bytes | None = None, configOverrides: Sequence[tuple[str, str]] = ()) -> GitRunResult:
        """
        Run 'git --no-optional-locks [-c key=value]... -C workingDirectory args...' and return its exit code and output.
        A non-zero exit code is returned, not raised. A GitRunError is raised if git could not be started or did not finish in time.
        """
        command = [self.GitExecutable, "--no-optional-locks"]
        for key, value in configOverrides:
            command += ["-c", f"{key}={value}"]
        command += ["-C", workingDirectory]
        command += args

        environment = CreateGitEnvironment(dict(os.environ))
        try:
            if stdinData is not None:
                result = subprocess.run(command, input=stdinData, capture_output=True, timeout=self.TimeoutSeconds, check=False, env=environment)
            else:
                # git gets an empty stdin, so it never waits for the stdin of this process
                result = subprocess.run(command, stdin=subprocess.DEVNULL, capture_output=True, timeout=self.TimeoutSeconds, check=False, env=environment)
        except FileNotFoundError as ex:
            raise GitRunError(f"The git executable '{self.GitExecutable}' was not found", True) from ex
        except subprocess.TimeoutExpired as ex:
            raise GitRunError(f"'{self.GitExecutable}' did not finish within {self.TimeoutSeconds} seconds in '{workingDirectory}'", False) from ex
        except OSError as ex:
            raise GitRunError(f"'{self.GitExecutable}' could not be run: {ex}", False) from ex
        return GitRunResult(result.returncode, result.stdout, result.stderr.decode("utf-8", errors="replace"))
