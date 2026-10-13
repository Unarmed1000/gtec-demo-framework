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

# The project id a gen file declares for its package, recorded in the project id cache.
#
# The gen file is what counts, the cache follows it:
# - A package that was renamed or moved keeps the id of its gen file. The cache has that id under the old name: the entry moves to the
#   new name. No id is lost by this, so no project gets a new id.
# - A package whose gen file got another id gets that id in the cache.
# - Two packages of one run that declare the same id are an error: a project id belongs to one package.
# A name that is cached and not part of the run is not removed for that: a run loads the packages it needs, not all of them.

from FslBuildGen import Util
from FslBuildGen.Engine.Cache.JsonProjectIdCache import JsonProjectIdCache
from FslBuildGen.Exceptions import InvalidPackageNameException, UsageErrorException
from FslBuildGen.Log import Log

_g_verbosityLevel = 2


def Record(
    log: Log | None,
    cache: JsonProjectIdCache,
    projectIdToNameDict: dict[str, str],
    declaredNames: set[str],
    packageName: str,
    packageProjectId: str,
) -> None:
    """Record the id the gen file of a package declares.
    projectIdToNameDict: the cache seen from the id, kept in step with it here
    declaredNames: the packages whose declared id was recorded in this run, the package is added
    """
    if not Util.IsValidPackageName(packageName):
        raise InvalidPackageNameException(packageName)

    cachedName = projectIdToNameDict.get(packageProjectId)
    if cachedName is not None and cachedName != packageName:
        if cachedName in declaredNames:
            raise UsageErrorException(
                f"The packages '{cachedName}' and '{packageName}' both declare the project id '{packageProjectId}': a project id belongs to one "
                "package, give one of them another id"
            )
        # The id was cached for a package that does not declare it in this run: the package got another name, or it is somewhere else now
        if log is not None:
            log.LogPrintVerbose(_g_verbosityLevel, f"Project id cache: '{packageProjectId}' moves from '{cachedName}' to '{packageName}', which declares it")
        cache.Remove(cachedName)

    cachedId = cache.ProjectIdDict.get(packageName)
    if cachedId is not None and cachedId != packageProjectId:
        if log is not None:
            log.LogPrintVerbose(
                _g_verbosityLevel, f"Project id cache: '{packageName}' gets the id '{packageProjectId}' it declares in the place of '{cachedId}'"
            )
        projectIdToNameDict.pop(cachedId, None)

    cache.Add(packageName, packageProjectId)
    projectIdToNameDict[packageProjectId] = packageName
    declaredNames.add(packageName)
