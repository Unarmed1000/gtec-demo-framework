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

# The template sanity check of FslBuildNew ('FslBuildNew <template or *> * --SanityCheck on'): which of its packages are built.
#
# The check creates a package from each template and builds them. A template can be one half of a pair: its package uses a package the
# user makes by hand from another template, so the package of the template alone can not be built. Such a template says so in its
# 'Template.xml' (<Template Standalone="false"/>). The check still creates its package, which checks the template and the generation of
# the package, and leaves it out of the build.

from collections.abc import Sequence

from FslBuildGen.Xml.XmlNewTemplateFile import XmlNewTemplateFile


def GetTemplatesThatAreNotStandalone(templates: Sequence[XmlNewTemplateFile], templateNames: Sequence[str]) -> list[str]:
    """The names of templateNames, in that order, that name a template whose package can not be built alone. A template is found by its
    name without case, as a package is created from it. A name that is no template is not one of them: creating its package fails.
    """
    notStandaloneIds = {template.Id for template in templates if not template.Template.Standalone}
    return [name for name in templateNames if name.lower() in notStandaloneIds]


def FormatNotBuiltLine(templateName: str) -> str:
    """What the check says about a template it leaves out of the build"""
    return (
        f"Not building the sanity project of template '{templateName}': "
        'a package made from this template can not be built alone (Standalone="false" in its Template.xml)'
    )


def FormatNotBuiltSummary(templateNames: Sequence[str]) -> str:
    """The line after the build that names what was not part of it"""
    names = ", ".join(f"'{name}'" for name in templateNames)
    return f'Sanity projects that were created but not built (Standalone="false" in Template.xml): {names}'
