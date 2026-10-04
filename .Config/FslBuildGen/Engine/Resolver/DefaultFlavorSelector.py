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

# The default flavor of a root package (EngineResolveConfig.CreateDefaultFlavor): which of the instances of the root is built when the user
# did not say.
#
# A root package has an instance for every flavor combination that is possible. The default is the first of those instances, so it is a
# combination that exists:
# - Only the flavors that differ between the instances take part: the ones with more than one option among the instances.
# - The instances are ordered by their options for these flavors, flavor by flavor in the order of the flavor names. Names and options are
#   compared as text.
# - An instance that does not have a flavor (the flavor belongs to a dependency only another option pulls in) counts as having the first
#   option of that flavor: a constraint on a flavor an instance does not have never rules it out.
# The default constraints are the options of the first instance in that order, with the first option for the flavors it does not have.
#
# Before 3.14.56 the default was the first option of each flavor, each picked on its own. That is the same when an instance has all of them,
# and it is the combination above in every case the old pick could be resolved. When the options of two flavors are tied (option C of one
# needs option B of the other, option D needs option A) the old pick named a combination no instance has.
#
# The result does not depend on the order of the instances or on the order of the flavors inside an instance.

from collections.abc import Mapping, Sequence


def FindFlavorsThatDiffer(instanceFlavorSelections: Sequence[Mapping[str, str]]) -> dict[str, list[str]]:
    """The flavors that have more than one option among the instances: the flavor name and its options, both sorted as text.
    instanceFlavorSelections holds the flavor selections of each instance (flavor name -> option name).
    """
    flavorOptions: dict[str, set[str]] = {}
    for flavorSelections in instanceFlavorSelections:
        for flavorName, optionName in flavorSelections.items():
            flavorOptions.setdefault(flavorName, set()).add(optionName)
    return {flavorName: sorted(flavorOptions[flavorName]) for flavorName in sorted(flavorOptions) if len(flavorOptions[flavorName]) > 1}


def SelectDefaultFlavorOptions(instanceFlavorSelections: Sequence[Mapping[str, str]]) -> dict[str, str]:
    """The default option of every flavor that differs between the instances of a root (flavor name -> option name, in the order of the flavor
    names): the options of the first instance. Empty when no flavor differs.
    """
    flavorsThatDiffer = FindFlavorsThatDiffer(instanceFlavorSelections)
    if len(flavorsThatDiffer) <= 0:
        return {}
    # An instance as its option of each flavor that differs, the first option of the flavor where the instance does not have the flavor
    candidates = [
        [flavorSelections.get(flavorName, options[0]) for flavorName, options in flavorsThatDiffer.items()] for flavorSelections in instanceFlavorSelections
    ]
    return dict(zip(flavorsThatDiffer.keys(), min(candidates), strict=True))


def FormatFlavorDefaults(rootFlavorDefaults: Mapping[str, Mapping[str, str]]) -> list[tuple[str, str | None]]:
    """What the user is told about the defaults that were selected (root package name -> flavor name -> option name).
    The root packages that got the same defaults share a line, so a tree where every package gets the same default prints one line:
    - one root:      "Flavor defaults for 'Top': Lib.Mode=A, Top.Own=D"
    - several roots: "Flavor defaults for 3 packages: Lib.Mode=A", and with it the line that names them: "- 'A', 'B', 'C'"
    Returns (line, the line that names the roots or None for one root). The flavors of a line and the root names are in the order of their
    names and the lines in the order of their first root name, all compared as text. A root without defaults is not part of any line.
    """
    # the defaults -> the roots that got them, the roots are added in the order of their names
    groups: dict[tuple[tuple[str, str], ...], list[str]] = {}
    for rootPackageName, defaults in sorted(rootFlavorDefaults.items()):
        if len(defaults) > 0:
            groups.setdefault(tuple(sorted(defaults.items())), []).append(rootPackageName)

    lines: list[tuple[str, str | None]] = []
    for defaultOptions, rootPackageNames in groups.items():
        strDefaults = ", ".join(f"{flavorName}={optionName}" for flavorName, optionName in defaultOptions)
        if len(rootPackageNames) == 1:
            lines.append((f"Flavor defaults for '{rootPackageNames[0]}': {strDefaults}", None))
        else:
            strRootPackageNames = ", ".join(f"'{rootPackageName}'" for rootPackageName in rootPackageNames)
            lines.append((f"Flavor defaults for {len(rootPackageNames)} packages: {strDefaults}", f"- {strRootPackageNames}"))
    return lines
