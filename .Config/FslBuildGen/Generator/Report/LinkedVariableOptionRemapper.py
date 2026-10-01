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

from FslBuildGen.Generator.Report.VariableReport import VariableReport


class LinkedVariableCanNotFollowOptionsException(Exception):
    def __init__(
        self, name: str, options: list[str], linkTargetName: str | None, masterName: str, masterOptionWithoutValue: str, newMasterOptions: list[str]
    ) -> None:
        super().__init__(
            f"The variable '{name}' is linked to '{linkTargetName}' and has no option for the '{masterName}' option '{masterOptionWithoutValue}', "
            f"so its options {options} can not follow the new '{masterName}' options {newMasterOptions}"
        )


class LinkedVariableOptionRemapper:
    """Keeps the variables that are linked to a master variable paired with it when the option list of the master is replaced.

    A linked variable pairs its options with the options of its master by index (the master is the variable at the end of its link chain), so
    when the master gets a new option list each linked option has to move to the index its master option has in the new list:
    - A master option that is in both lists keeps its linked option (the pairing follows the option name).
    - A new master option takes over the linked option at its own index when the master option that was at that index is gone. That is a rename,
      it is what pairing by index always did.
    - Any other new master option has no linked option. The linked option list can only miss options at its end (it is then shorter than the
      master list, like it is when an option is appended to the master list), a missing option in front of a paired one can not be expressed.
    - The linked option of a master option that is gone, and whose index was not taken over, is gone too.
    """

    @staticmethod
    def RemapLinkedVariables(variables: list[VariableReport], master: VariableReport, newMasterOptions: list[str]) -> list[VariableReport]:
        """The replacement of every variable that is linked to master, directly or through a chain of links, and whose options change when the
        master options are replaced by newMasterOptions. The variables that already match the new master options are not part of the result.

        variables: all variables in the order they were registered (a variable is always registered after its link target).
        Raises LinkedVariableCanNotFollowOptionsException when a linked variable can not follow, nothing is modified.
        """
        res: list[VariableReport] = []
        linkedNames: set[str] = {master.Name}
        for variable in variables:
            if variable.LinkTargetName is not None and variable.LinkTargetName in linkedNames:
                linkedNames.add(variable.Name)
                newOptions = LinkedVariableOptionRemapper.RemapOptions(variable, master, newMasterOptions)
                if newOptions != variable.Options:
                    res.append(VariableReport(variable.Name, newOptions, variable.LinkTargetName))
        return res

    @staticmethod
    def RemapOptions(variable: VariableReport, master: VariableReport, newMasterOptions: list[str]) -> list[str]:
        """The options of the linked variable in the order of newMasterOptions (see the class description)"""
        res: list[str] = []
        firstOptionWithoutValue: str | None = None
        for newIndex, masterOption in enumerate(newMasterOptions):
            sourceIndex = LinkedVariableOptionRemapper.__TryGetSourceIndex(master.Options, newMasterOptions, newIndex, masterOption)
            if sourceIndex is None or sourceIndex >= len(variable.Options):
                if firstOptionWithoutValue is None:
                    firstOptionWithoutValue = masterOption
            elif firstOptionWithoutValue is not None:
                # A linked option after a missing one would be paired with the wrong master option
                raise LinkedVariableCanNotFollowOptionsException(
                    variable.Name, variable.Options, variable.LinkTargetName, master.Name, firstOptionWithoutValue, newMasterOptions
                )
            else:
                res.append(variable.Options[sourceIndex])
        if firstOptionWithoutValue is not None and len(res) <= 0:
            # A variable can not have an empty option list
            raise LinkedVariableCanNotFollowOptionsException(
                variable.Name, variable.Options, variable.LinkTargetName, master.Name, firstOptionWithoutValue, newMasterOptions
            )
        return res

    @staticmethod
    def __TryGetSourceIndex(oldMasterOptions: list[str], newMasterOptions: list[str], newIndex: int, masterOption: str) -> int | None:
        """The index the master option had in the old list. For a new master option it is its own index when the old option there is gone."""
        if masterOption in oldMasterOptions:
            return oldMasterOptions.index(masterOption)
        if newIndex < len(oldMasterOptions) and oldMasterOptions[newIndex] not in newMasterOptions:
            return newIndex
        return None
