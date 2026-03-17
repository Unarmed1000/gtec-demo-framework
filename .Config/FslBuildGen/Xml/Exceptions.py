#!/usr/bin/env python3
# ****************************************************************************************************************************************************
# Copyright (c) 2014 Freescale Semiconductor, Inc.
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
#    * Neither the name of the Freescale Semiconductor, Inc. nor the names of
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

import xml.etree.ElementTree as ET

from FslBuildGen import PackageConfig


class XmlException(Exception):
    """Error"""


class XmlException2(Exception):
    def __init__(self, message: str) -> None:
        super().__init__(f"{message}")


class XmlRequiredAttributeMissingException(XmlException2):
    def __init__(self, xmlElement: ET.Element, attribName: str) -> None:
        msg = f"Element '{xmlElement.tag}' did not contain the required '{attribName}' attribute"
        super().__init__(msg)

        # raise XmlDuplicatedCompilerConfigurationException(result[config.Id].BasedOn, result[config.Id].Name, config.XMLElement, config.Name)


class XmlDuplicatedCompilerConfigurationException(XmlException2):
    def __init__(self, xmlElement1: ET.Element, name1: str, xmlElement2: ET.Element, name2: str) -> None:
        msg = f"Compiler comfigured multiple times as '{name1}' and '{name2}'"
        super().__init__(msg)


class XmlUnsupportedCompilerVersionException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str, version: str, validVersions: str) -> None:
        msg = f"Compiler '{name}' does not support version '{version}', expected '{validVersions}'"
        super().__init__(msg)


class XmlUnsupportedPlatformException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = "Platform name: '{}' is not a valid platform name, expected: {}".format(name, ", ".join(PackageConfig.APPROVED_PLATFORM_NAMES))
        super().__init__(msg)


class XmlMissingWindowsVisualStudioProjectIdException(XmlException2):
    def __init__(self, xmlElement: ET.Element, packageName: str) -> None:
        msg = f"The windows platform requires a ProjectId to be defined. Package name '{packageName}'"
        super().__init__(msg)


class XmlInvalidPackageNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, correctName: str, invalidName: str, strPath: str, strPathRoot: str) -> None:
        msg = f"The name: '{invalidName}' does not match the expected name of '{correctName}' for package at {strPath} with a package root of {strPathRoot}"
        super().__init__(msg)


class XmlInvalidSubPackageNameException(XmlInvalidPackageNameException):
    def __init__(self, xmlElement: ET.Element, correctName: str, invalidName: str, strPath: str, strPathRoot: str) -> None:
        # pylint: disable=useless-super-delegation
        super().__init__(xmlElement, correctName, invalidName, strPath, strPathRoot)


class XmlInvalidSubPackageShortNameConflictException(XmlException2):
    def __init__(self, xmlElement: ET.Element, correctName: str, invalidName: str) -> None:
        msg = f"The short sub package name: '{invalidName}' is not allowed because a directory called '{correctName}' exist"
        super().__init__(msg)


class XmlUnsupportedPackageNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The name: '{name}' contains unsupported characters"
        super().__init__(msg)


class XmlUnsupportedFlavorNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The flavor name: '{name}' contains unsupported characters"
        super().__init__(msg)


class XmlUnsupportedVariantNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The variant name: '{name}' contains unsupported characters"
        super().__init__(msg)


class XmlRequirementNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The requirement name: '{name}' contains unsupported characters"
        super().__init__(msg)


class XmlRequirementStringException(XmlException2):
    def __init__(self, xmlElement: ET.Element, desc: str, name: str) -> None:
        msg = f"The requirement {desc}: '{name}' contains unsupported characters"
        super().__init__(msg)


class XmlRequirementTypeException(XmlException2):
    def __init__(self, xmlElement: ET.Element, entryName: str, entryType: str, entryExtends: str, validList: list[str]) -> None:
        msg = "The requirement type: '{}' is invalid for '{}' extends '{}', expected [{}]".format(entryType, entryName, entryExtends, ", ".join(validList))
        super().__init__(msg)


class XmlRequirementTypeExtensionRequiresAValidExtendFieldException(XmlException2):
    def __init__(self, xmlElement: ET.Element, entryName: str) -> None:
        msg = f"The requirement '{entryName}' with type of 'extension' requires a non empty 'Extends' attribute"
        super().__init__(msg)


class XmlUnsupportedVirtualVariantNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The variant name: '{name}' contains unsupported characters"
        super().__init__(msg)


class XmlUnsupportedTag(XmlException2):
    def __init__(self, xmlElement: ET.Element, message: str) -> None:
        # pylint: disable=useless-super-delegation
        super().__init__(message)


class XmlUnsupportedFlavorOptionNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The flavor option name: '{name}' contains unsupported characters"
        super().__init__(msg)


class XmlUnsupportedVariantOptionNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The variant option name: '{name}' contains unsupported characters"
        super().__init__(msg)


class XmlInvalidVirtualVariantOptionException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"Variant '{name}'. A virtual variant must contain exactly one option."
        super().__init__(msg)


class XmlUnsupportedVirtualVariantOptionNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, variantName: str, name: str) -> None:
        msg = f"The virtual variant option name: '{variantName}' is not equal to the variant name '{name}'"
        super().__init__(msg)


class XmlUnsupportedSubPackageNameException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The name: '{name}' is a invalid sub package name"
        super().__init__(msg)


class XmlFlavorOptionNameCollisionException(XmlException2):
    def __init__(self, xmlElement: ET.Element, firstName: str, secondName: str) -> None:
        msg = f"The option name: '{secondName}' collides with the previously defined '{firstName}'"
        super().__init__(msg)


class UnknownBuildCustomizationException(XmlException2):
    def __init__(self, xmlElement: ET.Element) -> None:
        msg = f"The build customization name: '{xmlElement.tag}' is not valid."
        super().__init__(msg)


class DefaultValueAlreadyDefinedException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The default value name: '{name}' has already been defined."
        super().__init__(msg)


class UnknownDefaultValueException(XmlException2):
    def __init__(self, xmlElement: ET.Element) -> None:
        msg = f"The default value name: '{xmlElement.tag}' is not valid."
        super().__init__(msg)


class XmlFormatException(Exception):
    """Indicate that a error exist in the config xml"""


class ImportTemplateNotFoundException(XmlException2):
    def __init__(self, xmlElement: ET.Element, templateName: str) -> None:
        msg = f"'{xmlElement.tag}' tries to import unknown template '{templateName}'"
        super().__init__(msg)


class XmlInvalidRootElement(Exception):
    """Error"""


class XmlUnsupportedPackageType(Exception):
    """Error"""


class PlatformAlreadyDefinedException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The platform name: '{name}' has already been defined."
        super().__init__(msg)


class BuildCustomizationAlreadyDefinedException(XmlException2):
    def __init__(self, xmlElement: ET.Element, name: str) -> None:
        msg = f"The build customization name: '{name}' has already been defined."
        super().__init__(msg)
