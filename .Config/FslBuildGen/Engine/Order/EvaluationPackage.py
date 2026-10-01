#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright 2020 NXP
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
#    * Neither the name of the NXP. nor the names of
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


from FslBuildGen.Engine.PackageFlavorName import PackageFlavorName
from FslBuildGen.Engine.PackageFlavorOptionName import PackageFlavorOptionName
from FslBuildGen.Engine.Unresolved.UnresolvedBasicPackage import UnresolvedBasicPackage
from FslBuildGen.Engine.Unresolved.UnresolvedPackageName import UnresolvedPackageName
from FslBuildGen.Exceptions import CircularDependencyException, GroupedException


class FlavorInfo:
    def __init__(self, flavorName: PackageFlavorName, flavorOption: PackageFlavorOptionName) -> None:
        super().__init__()
        self.FlavorName = flavorName
        self.FlavorOption = flavorOption


class EvaluationPackage:
    class DependencyRecord:
        def __init__(self, package: EvaluationPackage, flavorInfo: FlavorInfo | None) -> None:
            super().__init__()
            self.Package = package
            self.FlavorInfo = flavorInfo

    def __init__(self, name: UnresolvedPackageName, source: UnresolvedBasicPackage, directDependencies: list[DependencyRecord]) -> None:
        super().__init__()
        self.Name = name
        self.SourcePackage = source
        self.DirectDependencies = directDependencies
        self.IsSealed = False

    def Seal(self) -> None:
        if self.IsSealed:
            raise Exception("already sealed")
        self.IsSealed = True
        EvaluationPackage.__SanityCheckDependencies(self.DirectDependencies, self.Name)

    def __str__(self) -> str:
        return str(self.Name)

    def __repr__(self) -> str:
        return f"EvaluationPackage({self.Name})"

    @staticmethod
    def __SanityCheckDependencies(directDependencies: list[DependencyRecord], name: UnresolvedPackageName) -> None:
        if len(directDependencies) <= 0:
            return

        uniquePackageDict: dict[UnresolvedPackageName, list[EvaluationPackage.DependencyRecord]] = {}
        for record in directDependencies:
            if record.Package.Name == name:
                raise CircularDependencyException(f"Can not add dependency to self '{record.Package.Name}'")
            if record.Package.Name not in uniquePackageDict:
                recordList: list[EvaluationPackage.DependencyRecord] = []
                uniquePackageDict[record.Package.Name] = recordList
            else:
                recordList = uniquePackageDict[record.Package.Name]
            recordList.append(record)

        exceptionList: list[Exception] | None = None
        for pairKey, pairValue in uniquePackageDict.items():
            exception = EvaluationPackage.__TryCreateDuplicatedDependencyException(name, pairKey, pairValue)
            if exception is not None:
                if exceptionList is None:
                    exceptionList = []
                exceptionList.append(exception)

        if exceptionList is not None and len(exceptionList) > 0:
            if len(exceptionList) > 1:
                raise GroupedException(exceptionList)
            else:
                raise exceptionList[0]

    @staticmethod
    def __TryCreateDuplicatedDependencyException(
        fromPackageName: UnresolvedPackageName, toPackageName: UnresolvedPackageName, entries: list[EvaluationPackage.DependencyRecord]
    ) -> Exception | None:
        if len(entries) <= 1:
            return None

        # The only allowed collisions are dependencies from different options of the same flavor, as an instance selects exactly one option per flavor.
        # Two flavors (or a flavor and a flavor extension) that depend on the same package would give the instances that select both a duplicate.
        flavorNames: set[PackageFlavorName] = set()
        flavorOptionNames: set[PackageFlavorOptionName] = set()
        for entry in entries:
            if entry.FlavorInfo is None:
                return EvaluationPackage.__CreateDuplicatedDependencyException(fromPackageName, toPackageName, entries)
            flavorNames.add(entry.FlavorInfo.FlavorName)
            flavorOptionNames.add(entry.FlavorInfo.FlavorOption)

        if len(flavorNames) > 1:
            helpStr = EvaluationPackage.__ToDuplicatedDependencyHelp(fromPackageName, toPackageName, entries)
            return Exception(
                f"Package '{fromPackageName}' has duplicate dependency to '{toPackageName}' from different flavors: [{helpStr}], only the options of one flavor can depend on the same package"
            )
        if len(flavorOptionNames) != len(entries):
            return EvaluationPackage.__CreateDuplicatedDependencyException(fromPackageName, toPackageName, entries)
        return None

    @staticmethod
    def __CreateDuplicatedDependencyException(
        fromPackageName: UnresolvedPackageName, toPackageName: UnresolvedPackageName, entries: list[EvaluationPackage.DependencyRecord]
    ) -> Exception:
        helpStr = EvaluationPackage.__ToDuplicatedDependencyHelp(fromPackageName, toPackageName, entries)
        return Exception(f"Package '{fromPackageName}' has duplicate dependency to '{toPackageName}': [{helpStr}]")

    @staticmethod
    def __ToDuplicatedDependencyHelp(
        fromPackageName: UnresolvedPackageName, toPackageName: UnresolvedPackageName, entries: list[EvaluationPackage.DependencyRecord]
    ) -> str:
        res: list[str] = []
        for entry in entries:
            desc = (
                f"{fromPackageName}<{entry.FlavorInfo.FlavorName}={entry.FlavorInfo.FlavorOption}>->{toPackageName}"
                if entry.FlavorInfo is not None
                else f"{fromPackageName}->{toPackageName}"
            )
            res.append(desc)
        return ", ".join(res)
