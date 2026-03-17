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


from FslBuildGen.DataTypes import AccessType, ExternalDependencyType
from FslBuildGen.PackageIncludeDir import PackageIncludeDir
from FslBuildGen.Packages.Unresolved.UnresolvedExternalDependencyPackageManager import UnresolvedExternalDependencyPackageManager
from FslBuildGen.SemanticVersionPattern import SemanticVersionPattern


class UnresolvedExternalDependency:
    def __init__(
        self,
        name: str,
        debugName: str,
        targetName: str,
        includeDir: PackageIncludeDir | None,
        location: str | None,
        hintPath: str | None,
        version: SemanticVersionPattern | None,
        publicKeyToken: str | None,
        processorArchitecture: str | None,
        culture: str | None,
        packageManager: UnresolvedExternalDependencyPackageManager | None,
        ifCondition: str | None,
        elementType: ExternalDependencyType,
        accessType: AccessType,
        isManaged: bool = False,
    ) -> None:
        super().__init__()
        self.Name = name
        self.DebugName = debugName if debugName is not None else name
        self.TargetName = targetName if targetName is not None else f"{name}::{name}"
        self.IncludeDir = includeDir
        self.Location = location
        self.HintPath = hintPath
        self.Version = version
        self.PublicKeyToken = publicKeyToken
        self.ProcessorArchitecture = processorArchitecture
        self.Culture = culture
        self.PackageManager = packageManager
        self.IfCondition: str | None = ifCondition
        # Can only be set from code, and it indicates that this dependency is managed by a recipe or similar
        self.Type: ExternalDependencyType = elementType

        self.Access = accessType
        self.IsManaged = isManaged

        if self.Type == ExternalDependencyType.DLL:
            if self.IncludeDir is not None:
                raise Exception(f"DLL dependency: '{self.Name}' can not contain include paths")
            if self.Access != AccessType.Public:
                raise Exception(f"DLL dependency: '{self.Name}' can only have a access type of Public")
