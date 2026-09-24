#!/usr/bin/env python3

# ****************************************************************************************************************************************************
# Copyright (c) 2016 Freescale Semiconductor, Inc.
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

import ast
import copy
from typing import Any

from FslBuildGen.Generator.Report.GeneratorVariableReport import GeneratorVariableReport
from FslBuildGen.Generator.Report.ReportVariableFormatter import ReportVariableFormatter

# import sys

# We use the python AST to do a safe condition evaluation
# Example
#  Expression
#    BoolOp
#      And
#      BoolOp
#        Or
#        Name
#          Load
#        Name
#          Load
#      UnaryOp
#        Not
#        Name
#          Load


class LocalNodeVisitor(ast.NodeVisitor):
    def __init__(self, contentProcessorName: str, source: str) -> None:
        self.ContentProcessorName = contentProcessorName
        self.Source = source
        self.Indent = 0

    def CheckNodeType(self, node: Any) -> None:
        if not (isinstance(node, (ast.Expression, ast.BoolOp, ast.UnaryOp, ast.Not, ast.Or, ast.And, ast.Name, ast.Load))):
            if hasattr(node, "col_offset"):
                if hasattr(node, "id"):
                    raise Exception(
                        f"FeatureRequirements contain unsupported node type '{type(node).__name__}' at '{node.id} (x:{node.col_offset})' in content builder '{self.ContentProcessorName}' feature requirement: '{self.Source}'"
                    )
                else:
                    raise Exception(
                        f"FeatureRequirements contain unsupported node type '{type(node).__name__}' at (x:{node.col_offset}) in content builder '{self.ContentProcessorName}' feature requirement: '{self.Source}'"
                    )
            else:
                raise Exception(
                    f"FeatureRequirements contain unsupported node type '{type(node).__name__}' in content builder '{self.ContentProcessorName}' feature requirement: '{self.Source}'"
                )


class ConditionNodeVisitor(LocalNodeVisitor):
    def __init__(self, contentProcessorName: str, source: str) -> None:
        super().__init__(contentProcessorName, source)

    def generic_visit(self, node: Any) -> None:
        self.CheckNodeType(node)
        # print(" "* (self.Indent*2) + type(node).__name__)
        self.Indent = self.Indent + 1
        ast.NodeVisitor.generic_visit(self, node)
        self.Indent = self.Indent - 1


class ConditionInterpreterNodeTransformer(ast.NodeTransformer):
    def __init__(self, featureIds: list[str]) -> None:
        self.__FeatureIds = featureIds

    def visit_Name(self, node: Any) -> Any:
        featureInList: bool = node.id.lower() in self.__FeatureIds
        val = ast.Constant(1 if featureInList else 0)
        return ast.copy_location(val, node)


class EvaluateLocalNodeVisitor(ast.NodeVisitor):
    def __init__(self, sourceName: str, source: str) -> None:
        self.SourceName = sourceName
        self.Source = source
        self.Indent = 0

    def CheckNodeType(self, node: Any) -> None:
        if not (
            isinstance(
                node,
                (
                    ast.Expression,
                    ast.BoolOp,
                    ast.Constant,
                    ast.UnaryOp,
                    ast.Not,
                    ast.NotEq,
                    ast.Eq,
                    ast.Or,
                    ast.And,
                    ast.Name,
                    ast.Compare,
                    ast.Load,
                ),
            )
        ):
            if hasattr(node, "col_offset"):
                if hasattr(node, "id"):
                    raise Exception(
                        f"Evaluate contain unsupported node type '{type(node).__name__}' at '{node.id} (x:{node.col_offset})' in Evaluate '{self.SourceName}' condition: '{self.Source}'"
                    )
                else:
                    raise Exception(
                        f"Evaluate contain unsupported node type '{type(node).__name__}' at (x:{node.col_offset}) in Evaluate '{self.SourceName}' condition: '{self.Source}'"
                    )
            else:
                raise Exception(f"Evaluate contain unsupported node type '{type(node).__name__}' in Evaluate '{self.SourceName}' condition: '{self.Source}'")


class EvaluateConditionNodeVisitor(EvaluateLocalNodeVisitor):
    def __init__(self, sourceName: str, source: str) -> None:
        super().__init__(sourceName, source)

    def generic_visit(self, node: Any) -> None:
        self.CheckNodeType(node)
        # print(" "* (self.Indent*2) + type(node).__name__)
        self.Indent = self.Indent + 1
        ast.NodeVisitor.generic_visit(self, node)
        self.Indent = self.Indent - 1


# class EvaluateConditionInterpreterNodeTransformer(ast.NodeTransformer):
#    def __init__(self, validVariableDict: Dict[str, str]) -> None:
#        self.__ValidVariableDict = validVariableDict

#    def visit_Name(self, node: Any) -> Any:
#        raise Exception("qw")
#        #featureInList = node.id.lower() in self.__FeatureIds # type: bool

#        # workaround the issue that ast.Num no longer accepts a bool in python3 and
#        # ast.Constant is python 3.6+
#        #val = ast.Num(1 if featureInList else 0)

#        #return ast.copy_location(val, node)


class ConditionInterpreter:
    def __init__(self, contentProcessorName: str, featureRequirements: str) -> None:
        self.__ContentProcessorName = contentProcessorName
        self.__FeatureRequirements = featureRequirements

        # Do some validation on the condition to ensure it only contains the elements we want and support
        astRootNode = self.__Parse(featureRequirements)
        nodeVisitor = ConditionNodeVisitor(contentProcessorName, featureRequirements)
        nodeVisitor.visit(astRootNode)
        self.__RootNode = astRootNode

    def __Parse(self, featureRequirements: str) -> ast.Expression:
        try:
            return ast.parse(featureRequirements, mode="eval")
        except SyntaxError as ex:
            raise Exception(
                f"FeatureRequirements {ex.msg} in ContentBuilder '{self.__ContentProcessorName}' at 'x:{ex.offset}' in '{str(ex.text).strip()}' "
            ) from ex

    def CheckFeatureRequirements(self, featuresIds: list[str]) -> bool:
        astRootNode = copy.deepcopy(self.__RootNode)
        nodeTransformer = ConditionInterpreterNodeTransformer(featuresIds)
        nodeTransformer.visit(astRootNode)
        ast.fix_missing_locations(astRootNode)
        codeobj = compile(astRootNode, "<string>", mode="eval")
        # this should be safe as the root ast tree object has been verified to only contain things we expect
        return bool(eval(codeobj) == 1)


class EvaluateConditionInterpreter:
    @staticmethod
    def Evaluate(condition: str, validVariableDict: dict[str, object], source: str) -> bool:
        variableReport = GeneratorVariableReport()
        for variableKey, variableValue in validVariableDict.items():
            if isinstance(variableValue, str):
                variableReport.Add(variableKey, [f"'{variableValue}'"])
            elif isinstance(variableValue, bool):
                variableReport.Add(variableKey, [f"{variableValue}"])
            else:
                raise Exception("Not supported")
        processedCondition = ReportVariableFormatter.Format2(condition, variableReport, {})

        # Do some validation on the condition to ensure it only contains the elements we want and support
        astRootNode = EvaluateConditionInterpreter.__Parse(processedCondition, condition, source)
        nodeVisitor = EvaluateConditionNodeVisitor(source, processedCondition)
        nodeVisitor.visit(astRootNode)
        return EvaluateConditionInterpreter.__DoEvaluate(astRootNode, validVariableDict)

    @staticmethod
    def __Parse(condition: str, sourceCondition: str, source: str) -> ast.Expression:
        try:
            return ast.parse(condition, mode="eval")
        except SyntaxError as ex:
            raise Exception(f"Evaluate({ex.msg}) from '{source}' at 'x:{ex.offset}' in '{str(ex.text).strip()}' ") from ex

    @staticmethod
    def __DoEvaluate(astRootNode: ast.Expression, validVariableDict: dict[str, object]) -> bool:
        # nodeTransformer = EvaluateConditionInterpreterNodeTransformer(validVariableDict)
        # nodeTransformer.visit(astRootNode)
        ast.fix_missing_locations(astRootNode)
        codeobj = compile(astRootNode, "<string>", mode="eval")
        # this should be safe as the root ast tree object has been verified to only contain things we expect
        return bool(eval(codeobj) == 1)
