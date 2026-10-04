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


from collections.abc import Callable

from FslBuildGen.Engine.Order.Exceptions import PackageHasNoValidFlavorCombinationException
from FslBuildGen.Engine.PackageFlavorName import PackageFlavorName
from FslBuildGen.Engine.PackageFlavorOptionName import PackageFlavorOptionName
from FslBuildGen.Engine.PackageFlavorSelection import PackageFlavorSelection
from FslBuildGen.Engine.PackageFlavorSelections import PackageFlavorSelections, PackageFlavorSelectionsEmpty
from FslBuildGen.Engine.Resolver.InstanceConfig import InstanceConfig
from FslBuildGen.Engine.Resolver.PackageDependency import PackageDependency
from FslBuildGen.Engine.Resolver.PackageName import PackageName
from FslBuildGen.Engine.Resolver.ResolvedPackageTemplate import (
    ResolvedPackageFlavor,
    ResolvedPackageFlavorExtension,
    ResolvedPackageFlavorOption,
    ResolvedPackageTemplate,
    ResolvedPackageTemplateDependency,
)
from FslBuildGen.Engine.Unresolved.UnresolvedBasicPackage import UnresolvedBasicPackage
from FslBuildGen.Engine.Unresolved.UnresolvedPackageDependency import UnresolvedPackageDependency
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavor import UnresolvedPackageFlavor
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavorExtension import UnresolvedPackageFlavorExtension
from FslBuildGen.Engine.Unresolved.UnresolvedPackageFlavorOption import UnresolvedPackageFlavorOption
from FslBuildGen.Log import Log

# from FslBuildGen.Resolver.PackageFlavorSelections import PackageFlavorSelections


class LocalVerbosityLevel:
    Trace = 6


class Record:
    def __init__(self, packageTemplate: ResolvedPackageTemplate) -> None:
        super().__init__()
        self.PackageTemplate = packageTemplate


class PackageResolver:
    def __init__(self, log: Log) -> None:
        super().__init__()
        self.__PackageTemplateDict: dict[str, Record] = {}
        self.__Log = log

    def Resolve(self, unresolvedPackage: UnresolvedBasicPackage) -> ResolvedPackageTemplate:
        # Since we are resolving this package, it should not be present in the dict yet
        if unresolvedPackage.Name.Value in self.__PackageTemplateDict:
            raise Exception(f"Internal error package '{unresolvedPackage.Name}' already resolved")

        directDependencies = self.__ResolveDirectDependencies(unresolvedPackage)
        allInstancesConfigurations = self.__GenerateInstancesConfigurations(unresolvedPackage)

        packageFlavors: list[ResolvedPackageFlavor] = PackageResolver.__ResolveFlavors(unresolvedPackage, self.__PackageTemplateDict)
        packageFlavorExtensions: list[ResolvedPackageFlavorExtension] = PackageResolver.__ResolveFlavorExtensions(unresolvedPackage, self.__PackageTemplateDict)

        newTemplateName = PackageName.CreateName(unresolvedPackage.Name)
        newTemplate = ResolvedPackageTemplate(
            newTemplateName, unresolvedPackage.Type, directDependencies, allInstancesConfigurations, packageFlavors, packageFlavorExtensions
        )
        self.__PackageTemplateDict[newTemplate.Name.Value] = Record(newTemplate)

        if self.__Log.Verbosity >= LocalVerbosityLevel.Trace:
            self.__Log.LogPrint(f"- Name: {newTemplate.Name} Combinations: {len(newTemplate.InstanceConfigs)}")
            self.__Log.PushIndent()
            try:
                for instanceConfig in newTemplate.InstanceConfigs:
                    strDirectDependency: str = ", ".join([str(dep) for dep in instanceConfig.DirectDependencies])
                    self.__Log.LogPrint(f"- {unresolvedPackage.Name}<{instanceConfig.Description}> directDependencies: [{strDirectDependency}]")
            finally:
                self.__Log.PopIndent()
        return newTemplate

    @staticmethod
    def __ResolveFlavors(unresolvedPackage: UnresolvedBasicPackage, packageTemplateDict: dict[str, Record]) -> list[ResolvedPackageFlavor]:
        packageFlavors: list[ResolvedPackageFlavor] = []
        for flavor in unresolvedPackage.Flavors:
            options: list[ResolvedPackageFlavorOption] = []
            for flavorOption in flavor.Options:
                if len(flavorOption.DirectDependencies) > 0:
                    resolvedDependencies = []  # type List[ResolvedPackageTemplateDependency]
                    for srcDep in flavorOption.DirectDependencies:
                        if srcDep.Name.Value not in packageTemplateDict:
                            raise Exception(f"Unknown dependency '{srcDep.Name}'")
                        depRecord = packageTemplateDict[srcDep.Name.Value]
                        resolvedDependencies.append(ResolvedPackageTemplateDependency(depRecord.PackageTemplate, srcDep.FlavorConstraints))
                else:
                    resolvedDependencies = []

                options.append(ResolvedPackageFlavorOption(flavorOption.Name, resolvedDependencies, flavorOption.Supported))
            packageFlavors.append(ResolvedPackageFlavor(flavor.Name, flavor.QuickName, options, flavor.DefaultOptionName))
        return packageFlavors

    @staticmethod
    def __ResolveFlavorExtensions(unresolvedPackage: UnresolvedBasicPackage, packageTemplateDict: dict[str, Record]) -> list[ResolvedPackageFlavorExtension]:
        packageFlavorExtensions: list[ResolvedPackageFlavorExtension] = []
        for flavor in unresolvedPackage.FlavorExtensions:
            options: list[ResolvedPackageFlavorOption] = []
            for flavorOption in flavor.Options:
                if len(flavorOption.DirectDependencies) > 0:
                    resolvedDependencies = []  # type List[ResolvedPackageTemplateDependency]
                    for srcDep in flavorOption.DirectDependencies:
                        if srcDep.Name.Value not in packageTemplateDict:
                            raise Exception(f"Unknown dependency '{srcDep.Name}'")
                        depRecord = packageTemplateDict[srcDep.Name.Value]
                        resolvedDependencies.append(ResolvedPackageTemplateDependency(depRecord.PackageTemplate, srcDep.FlavorConstraints))
                else:
                    resolvedDependencies = []

                options.append(ResolvedPackageFlavorOption(flavorOption.Name, resolvedDependencies, flavorOption.Supported))
            packageFlavorExtensions.append(ResolvedPackageFlavorExtension(flavor.Name, options))
        return packageFlavorExtensions

    def __GenerateFlavorPermutations(
        self,
        instanceConfigs: list[InstanceConfig],
        permutation: list[PackageFlavorSelection],
        permutationDirectDependencies: list[PackageDependency],
        flavors: list[UnresolvedPackageFlavor],
        flavorIndex: int,
        unresolvedPackage: UnresolvedBasicPackage,
        flavorConstraints: PackageFlavorSelections,
    ) -> None:
        if flavorIndex >= len(flavors):
            # Every own flavor has an option, the selected flavor extension options add their dependencies last
            self.__GenerateFlavorExtensionPermutations(
                instanceConfigs, permutation, permutationDirectDependencies, unresolvedPackage, flavorConstraints, frozenset()
            )
            return

        flavor = flavors[flavorIndex]
        for flavorOption in flavor.Options:
            if self.__Log.Verbosity >= LocalVerbosityLevel.Trace:
                self.__Log.LogPrint(f"- Flavor {flavor.Name}={flavorOption.Name}")
            self.__Log.PushIndent()
            try:
                permutation.append(PackageFlavorSelection(flavor.Name, flavorOption.Name))
                self.__CombineDependencies(
                    permutation,
                    permutationDirectDependencies,
                    unresolvedPackage,
                    flavorConstraints,
                    flavorOption.DirectDependencies,
                    0,
                    lambda currentPermutation, currentConstraints: self.__GenerateFlavorPermutations(
                        instanceConfigs, currentPermutation, permutationDirectDependencies, flavors, flavorIndex + 1, unresolvedPackage, currentConstraints
                    ),
                )
                permutation.pop()
            finally:
                self.__Log.PopIndent()

    def __GenerateFlavorExtensionPermutations(
        self,
        instanceConfigs: list[InstanceConfig],
        permutation: list[PackageFlavorSelection],
        permutationDirectDependencies: list[PackageDependency],
        unresolvedPackage: UnresolvedBasicPackage,
        flavorConstraints: PackageFlavorSelections,
        appliedFlavorExtensions: frozenset[PackageFlavorName],
    ) -> None:
        """A permutation that selects an option of an extended flavor depends on the dependencies of the extension option. An instance of such a
        dependency can select another extended flavor, so the extensions are searched again after each one until no selected extension option with
        dependencies is left. Then the permutation becomes an instance config when it meets the constraints.
        """
        nextExtension = PackageResolver.__TryFindNextFlavorExtensionOption(unresolvedPackage, permutation, appliedFlavorExtensions)
        if nextExtension is None:
            self.__TryAddInstanceConfig(instanceConfigs, permutation, permutationDirectDependencies, unresolvedPackage, flavorConstraints)
            return

        flavorExtension, flavorOption = nextExtension
        if self.__Log.Verbosity >= LocalVerbosityLevel.Trace:
            self.__Log.LogPrint(f"- Flavor extension {flavorExtension.Name}={flavorOption.Name}")
        newAppliedFlavorExtensions = appliedFlavorExtensions | {flavorExtension.Name}
        self.__Log.PushIndent()
        try:
            self.__CombineDependencies(
                permutation,
                permutationDirectDependencies,
                unresolvedPackage,
                flavorConstraints,
                flavorOption.DirectDependencies,
                0,
                lambda currentPermutation, currentConstraints: self.__GenerateFlavorExtensionPermutations(
                    instanceConfigs, currentPermutation, permutationDirectDependencies, unresolvedPackage, currentConstraints, newAppliedFlavorExtensions
                ),
            )
        finally:
            self.__Log.PopIndent()

    @staticmethod
    def __TryFindNextFlavorExtensionOption(
        unresolvedPackage: UnresolvedBasicPackage, permutation: list[PackageFlavorSelection], appliedFlavorExtensions: frozenset[PackageFlavorName]
    ) -> tuple[UnresolvedPackageFlavorExtension, UnresolvedPackageFlavorOption] | None:
        """The first flavor extension (by name) that is not applied yet, whose flavor the permutation selects and whose selected option has dependencies"""
        for flavorExtension in unresolvedPackage.FlavorExtensions:
            if flavorExtension.Name not in appliedFlavorExtensions:
                index = PackageResolver.__IndexOf(permutation, flavorExtension.Name)
                if index >= 0:
                    flavorOption = flavorExtension.TryGetOptionByName(permutation[index].Option)
                    if flavorOption is not None and len(flavorOption.DirectDependencies) > 0:
                        return (flavorExtension, flavorOption)
        return None

    def __TryAddInstanceConfig(
        self,
        instanceConfigs: list[InstanceConfig],
        permutation: list[PackageFlavorSelection],
        permutationDirectDependencies: list[PackageDependency],
        unresolvedPackage: UnresolvedBasicPackage,
        flavorConstraints: PackageFlavorSelections,
    ) -> None:
        flavorSelections = PackageFlavorSelections(list(permutation))
        if PackageResolver.__IsAllowed(flavorSelections, flavorConstraints):
            instanceConfig = InstanceConfig(flavorSelections, list(permutationDirectDependencies))
            strDependencies = ", ".join([str(dep) for dep in instanceConfig.DirectDependencies])
            if self.__Log.Verbosity >= LocalVerbosityLevel.Trace:
                self.__Log.LogPrint(
                    f"- Package {unresolvedPackage.Name} InstanceConfig: '{instanceConfig.Description}' DirectDependencies: [{strDependencies}]"
                )
            instanceConfigs.append(instanceConfig)
        elif self.__Log.Verbosity >= LocalVerbosityLevel.Trace:
            self.__Log.LogPrint(
                f"- Package {unresolvedPackage.Name} Permutation: '{flavorSelections.Description}' rejected due to constraints: {flavorConstraints}"
            )

    def __CombineDependencies(
        self,
        permutation: list[PackageFlavorSelection],
        permutationDirectDependencies: list[PackageDependency],
        unresolvedPackage: UnresolvedBasicPackage,
        flavorConstraints: PackageFlavorSelections,
        directDependencies: list[UnresolvedPackageDependency],
        depIndex: int,
        onCombined: Callable[[list[PackageFlavorSelection], PackageFlavorSelections], None],
    ) -> None:
        """Pick an instance of each dependency in turn that agrees with the permutation and the constraints. For every combination of picked
        instances onCombined is called with the extended permutation and constraints while the picked instances are on permutationDirectDependencies.
        """
        if depIndex >= len(directDependencies):
            onCombined(permutation, flavorConstraints)
            return

        dependency: UnresolvedPackageDependency = directDependencies[depIndex]
        if dependency.Name.Value not in self.__PackageTemplateDict:
            raise Exception(f"Unknown dependency '{dependency.Name}'")
        depRecord = self.__PackageTemplateDict[dependency.Name.Value]

        # permutationHitCount = 0 # type: int
        # var rejectionReasons = new List<string>();
        for combination in depRecord.PackageTemplate.InstanceConfigs:
            if self.__Log.Verbosity >= LocalVerbosityLevel.Trace:
                self.__Log.LogPrint(f"- Dependency {dependency.Name} combination: '{combination.Description}'")

            variantFlavorConstraints = self.__TryCombineConstraints(flavorConstraints, dependency.FlavorConstraints)
            if variantFlavorConstraints is not None:
                currentPermutation = self.__TryCombine(permutation, combination)
                if currentPermutation is not None:
                    # permutationHitCount = permutationHitCount + 1
                    permutationDirectDependencies.append(
                        PackageDependency(PackageName.CreateUnresolvedNameAndSelection(dependency.Name, combination.FlavorSelections), dependency)
                    )

                    self.__CombineDependencies(
                        currentPermutation,
                        permutationDirectDependencies,
                        unresolvedPackage,
                        variantFlavorConstraints,
                        directDependencies,
                        depIndex + 1,
                        onCombined,
                    )
                    permutationDirectDependencies.pop()
                else:
                    strDescPermutation = ", ".join([str(entry) for entry in permutation])
                    if self.__Log.Verbosity >= LocalVerbosityLevel.Trace:
                        self.__Log.LogPrint(
                            f"  - Package '{unresolvedPackage.Name}' flavor rejected due to merge failure of '{strDescPermutation}' and '{combination.Description}' Dependency: {dependency.Name}"
                        )
            elif self.__Log.Verbosity >= LocalVerbosityLevel.Trace:
                self.__Log.LogPrint(
                    f"-  Package '{unresolvedPackage.Name}' flavor rejected due to constraint conflict: '{flavorConstraints}' and '{dependency.FlavorConstraints.Description}' Dependency: {dependency.Name}"
                )

        # if permutationHitCount <= 0:
        #  raise Exception(f"Package '{unresolvedPackage.Name}' unable to generate any valid permutations due to:\n{string.Join("\n  ", rejectionReasons)}")

    def __TryCombine(self, permutation: list[PackageFlavorSelection], combination: InstanceConfig) -> list[PackageFlavorSelection] | None:
        newPermutation: list[PackageFlavorSelection] = list(permutation)

        for entry in combination.FlavorSelections.Selections:
            index = PackageResolver.__IndexOf(newPermutation, entry.Name)
            if index < 0:
                # The flavor was not part of the permutation, so just add it
                newPermutation.append(entry)
            elif newPermutation[index].Option != entry.Option:
                # The flavor was part of the permutation but its not compatible -> abort
                return None
        return newPermutation

    def __ResolveDirectDependencies(self, unresolvedPackage: UnresolvedBasicPackage) -> list[ResolvedPackageTemplateDependency]:
        if len(unresolvedPackage.DirectDependencies) > 0:
            resolvedDependencies: list[ResolvedPackageTemplateDependency] = []
            for dependency in unresolvedPackage.DirectDependencies:
                if dependency.Name.Value not in self.__PackageTemplateDict:
                    raise Exception(f"Unknown dependency '{dependency.Name}'")
                depRecord = self.__PackageTemplateDict[dependency.Name.Value]
                # Combine
                resolvedDependencies.append(ResolvedPackageTemplateDependency(depRecord.PackageTemplate, dependency.FlavorConstraints))
        else:
            resolvedDependencies = []
        return resolvedDependencies

    def __GenerateInstancesConfigurations(self, unresolvedPackage: UnresolvedBasicPackage) -> list[InstanceConfig]:
        instanceConfigs: list[InstanceConfig] = []
        # The direct dependencies first, then the own flavors and finally the selected flavor extension options
        permutationDirectDependencies: list[PackageDependency] = []
        self.__CombineDependencies(
            [],
            permutationDirectDependencies,
            unresolvedPackage,
            PackageFlavorSelectionsEmpty.Empty,
            unresolvedPackage.DirectDependencies,
            0,
            lambda permutation, flavorConstraints: self.__GenerateFlavorPermutations(
                instanceConfigs, permutation, permutationDirectDependencies, unresolvedPackage.Flavors, 0, unresolvedPackage, flavorConstraints
            ),
        )

        # We started with combinations but ended with none, so we have a impossible to satisfy constraint. The constraints are validated while ordering
        # the packages, but a constraint inside a flavor or flavor extension option is not compared to the other constraints (different options may
        # constrain differently)
        if len(instanceConfigs) <= 0 and (len(unresolvedPackage.DirectDependencies) > 0 or len(unresolvedPackage.Flavors) > 0):
            raise PackageHasNoValidFlavorCombinationException(unresolvedPackage.Name, PackageResolver.__DescribeDependencyConstraints(unresolvedPackage))

        if len(instanceConfigs) <= 0:
            instanceConfigs.append(InstanceConfig(PackageFlavorSelectionsEmpty.Empty, []))

        return instanceConfigs

    @staticmethod
    def __DescribeDependencyConstraints(unresolvedPackage: UnresolvedBasicPackage) -> list[str]:
        """The flavor constraints on the direct, the flavor option and the flavor extension option dependencies as
        '<package>-><dependency>[<constraints>]' and '<package><<flavor>=<option>>-><dependency>[<constraints>]'
        """
        res: list[str] = [
            f"{unresolvedPackage.Name}->{dep.Name}[{dep.FlavorConstraints.Description}]"
            for dep in unresolvedPackage.DirectDependencies
            if len(dep.FlavorConstraints.Selections) > 0
        ]
        flavorOptions = [(flavor.Name, flavorOption) for flavor in unresolvedPackage.Flavors for flavorOption in flavor.Options]
        flavorOptions += [
            (flavorExtension.Name, flavorOption) for flavorExtension in unresolvedPackage.FlavorExtensions for flavorOption in flavorExtension.Options
        ]
        for flavorName, flavorOption in flavorOptions:
            res.extend(
                f"{unresolvedPackage.Name}<{flavorName}={flavorOption.Name}>->{dep.Name}[{dep.FlavorConstraints.Description}]"
                for dep in flavorOption.DirectDependencies
                if len(dep.FlavorConstraints.Selections) > 0
            )
        return res

    @staticmethod
    def __TryCombineConstraints(flavorConstraints: PackageFlavorSelections, depFlavorConstraints: PackageFlavorSelections) -> PackageFlavorSelections | None:
        if len(flavorConstraints.Selections) <= 0:
            return depFlavorConstraints
        if len(depFlavorConstraints.Selections) <= 0:
            return flavorConstraints

        constraintDict: dict[PackageFlavorName, PackageFlavorOptionName] = {}
        for entry in flavorConstraints.Selections:
            constraintDict[entry.Name] = entry.Option

        if not PackageResolver.__TryCombineConstraintsIntoDict(constraintDict, depFlavorConstraints.Selections):
            return None
        return PackageFlavorSelections([PackageFlavorSelection(name, option) for name, option in constraintDict.items()])

    @staticmethod
    def __TryCombineConstraintsIntoDict(
        constraintDict: dict[PackageFlavorName, PackageFlavorOptionName], constraintArray: list[PackageFlavorSelection]
    ) -> bool:
        for entry in constraintArray:
            if entry.Name not in constraintDict:
                constraintDict[entry.Name] = entry.Option
            elif entry.Option != constraintDict[entry.Name]:
                return False
        return True

    @staticmethod
    def __IsAllowed(flavorSelections: PackageFlavorSelections, requirements: PackageFlavorSelections) -> bool:
        if len(requirements.Selections) <= 0:
            return True

        for entry in flavorSelections.Selections:
            index = requirements.IndexOf(entry.Name)
            if index >= 0 and entry.Option != requirements.Selections[index].Option:
                return False
        return True

    @staticmethod
    def __IndexOf(entries: list[PackageFlavorSelection], entryName: PackageFlavorName) -> int:
        for i, entry in enumerate(entries):
            if entry.Name == entryName:
                return i
        return -1
