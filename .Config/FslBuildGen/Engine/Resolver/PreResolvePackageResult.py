#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#****************************************************************************************************************************************************
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
#****************************************************************************************************************************************************

from typing import List
from FslBuildGen.Engine.Resolver.ProcessedPackage import ProcessedPackage
from FslBuildGen.Packages.PackageRequirement import PackageRequirement

class PreResolvePackageResult(object):
    def __init__(self, sourcePackage: ProcessedPackage, resolvedPlatformSupported: bool,
                resolvedDirectRequirements: List[PackageRequirement], resolvedAllRequirements: List[PackageRequirement],
                resolvedDirectUsedFeatures: List[PackageRequirement], resolvedAllUsedFeatures: List[PackageRequirement],
                resolvedBuildIndex: int, resolvedBuildOrder: List['PreResolvePackageResult']) -> None:
        super().__init__()
        self.Type = sourcePackage.Type
        self.SourcePackage = sourcePackage
        self.ResolvedPlatformSupported = resolvedPlatformSupported
        self.ResolvedDirectRequirements = resolvedDirectRequirements
        self.ResolvedAllRequirements = resolvedAllRequirements
        self.ResolvedDirectUsedFeatures = resolvedDirectUsedFeatures
        self.ResolvedAllUsedFeatures = resolvedAllUsedFeatures
        self.ResolvedBuildIndex = resolvedBuildIndex
        self.ResolvedBuildOrder = resolvedBuildOrder

    def ContainsRecipe(self) -> bool:
        return self.SourcePackage.DirectExperimentalRecipe is not None

    def __str__(self) -> str:
        return 'SourcePackage.NameInfo:"{0}"'.format(self.SourcePackage.NameInfo)

    def __repr__(self) -> str:
        return self.__str__()
