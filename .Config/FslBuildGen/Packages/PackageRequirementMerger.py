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

from collections.abc import Iterable

from FslBuildGen.Exceptions import GroupedException
from FslBuildGen.Packages.ExceptionsXml import RequirementNameCollisionException
from FslBuildGen.Packages.PackageRequirement import PackageRequirement


class PackageRequirementMerger:
    """Merges the requirements of several packages into one entry per FullId (type, extends and name compared without case).

    The merged entries are copies, the requirements that are added are never modified. The IntroducedByPackages of a merged entry is the union of
    the added ones. A FullId that arrives spelled in more than one way (a name or extends that differs only by case) is a collision.
    """

    def __init__(self) -> None:
        super().__init__()
        # FullId -> the merged entry, in the order the FullIds were first added
        self.__mergedDict: dict[str, PackageRequirement] = {}
        # FullId -> (Name, Extends) -> merged entry of each spelling that differs from the one in __mergedDict
        self.__otherSpellingsDict: dict[str, dict[tuple[str, str], PackageRequirement]] = {}

    def Add(self, requirements: Iterable[PackageRequirement]) -> None:
        for requirement in requirements:
            merged = self.__mergedDict.get(requirement.FullId)
            if merged is None:
                self.__mergedDict[requirement.FullId] = requirement.CloneForMerge()
            elif merged.Name == requirement.Name and merged.Extends == requirement.Extends:
                merged.IntroducedByPackages.update(requirement.IntroducedByPackages)
            else:
                otherSpellings = self.__otherSpellingsDict.setdefault(requirement.FullId, {})
                spelling = (requirement.Name, requirement.Extends)
                otherMerged = otherSpellings.get(spelling)
                if otherMerged is None:
                    otherSpellings[spelling] = requirement.CloneForMerge()
                else:
                    otherMerged.IntroducedByPackages.update(requirement.IntroducedByPackages)

    def GetRequirements(self) -> list[PackageRequirement]:
        """The merged requirements in the order their FullId was first added.
        Raises RequirementNameCollisionException for a requirement that was spelled in several ways, a GroupedException when several were.
        """
        if len(self.__otherSpellingsDict) > 0:
            exceptionList: list[Exception] = [
                RequirementNameCollisionException([self.__mergedDict[fullId], *otherSpellings.values()])
                for fullId, otherSpellings in sorted(self.__otherSpellingsDict.items())
            ]
            raise exceptionList[0] if len(exceptionList) == 1 else GroupedException(exceptionList)
        return list(self.__mergedDict.values())
