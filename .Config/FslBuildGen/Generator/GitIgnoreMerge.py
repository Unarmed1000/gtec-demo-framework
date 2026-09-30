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

"""Merges the entries a package needs into its .gitignore without disturbing the lines that are already there.

Git reads a .gitignore from top to bottom and a later line can undo an earlier one ('build/*' followed by '!build/keep.txt'), so a file
somebody arranged by hand keeps the order of its lines, its comments and its blank lines, and the missing entries are appended at the end.

A file that is sorted and free of duplicates is the writer's own output (the writer used to sort every file), it stays sorted and the
missing entries are inserted at their sorted position. That keeps the generated files free of churn when a generator run for another
platform adds entries.
"""

import itertools
from collections.abc import Iterable


def SplitLines(content: str | None) -> list[str] | None:
    """Split the content of a .gitignore into lines, '\\r' is removed and the newline that ends the last line does not create an extra line.
    Returns None when there is no content (the file does not exist)."""
    if content is None:
        return None
    content = content.replace("\r", "")
    if len(content) == 0:
        return []
    lines = content.split("\n")
    if content.endswith("\n"):
        lines.pop()
    return lines


def JoinLines(lines: list[str]) -> str:
    """The .gitignore content for the lines, every line ends with a newline (no lines gives a single newline)."""
    return "\n".join(lines) + "\n"


def _IsBlank(line: str) -> bool:
    return len(line.strip()) == 0


def _AnchorTemplateEntry(entry: str) -> str:
    # Comments and negations are copied as they are, everything else is anchored to the package directory
    return entry if entry.startswith(("/", "!", "#")) else "/" + entry


def _AnchorGeneratorEntry(entry: str) -> str:
    return entry if entry.startswith("/") else "/" + entry


def _NormalizeGeneratorEntries(generatorEntries: Iterable[str]) -> list[str]:
    entries = [entry.replace("\r", "").strip() for entry in generatorEntries]
    return [entry for entry in entries if len(entry) > 0]


def GetRequiredEntries(templateEntries: list[str], generatorEntries: Iterable[str]) -> list[str]:
    """The entries a package .gitignore must contain: the template entries in template order followed by the generator entries sorted.
    Each entry appears once and blank lines are skipped."""
    anchoredGeneratorEntries = sorted({_AnchorGeneratorEntry(entry) for entry in _NormalizeGeneratorEntries(generatorEntries)})
    candidates = [_AnchorTemplateEntry(entry.replace("\r", "")) for entry in templateEntries if not _IsBlank(entry)]
    candidates += anchoredGeneratorEntries

    requiredEntries: list[str] = []
    seen: set[str] = set()
    for entry in candidates:
        key = entry.strip()
        # A lone '/' matches nothing, it was always filtered out
        if key != "/" and key not in seen:
            seen.add(key)
            requiredEntries.append(entry)
    return requiredEntries


def IsSortedAndUnique(lines: list[str]) -> bool:
    """True if the lines are in sorted order without duplicates, the form the writer used to give every file."""
    return all(previous < current for previous, current in itertools.pairwise(lines))


def _AnchorLegacyEntries(lines: list[str], legacyEntries: set[str]) -> tuple[list[str], bool]:
    """A legacy line 'name' is replaced in place by '/name', or dropped if '/name' is already present."""
    if len(legacyEntries) == 0:
        return lines, False
    changed = False
    present = {line.strip() for line in lines}
    anchoredLines: list[str] = []
    for line in lines:
        key = line.strip()
        if key in legacyEntries:
            changed = True
            anchoredEntry = "/" + key
            if anchoredEntry in present:
                continue
            present.add(anchoredEntry)
            line = anchoredEntry
        anchoredLines.append(line)
    return anchoredLines, changed


def MergeGitIgnoreLines(existing: list[str] | None, templateEntries: list[str], generatorEntries: Iterable[str]) -> list[str] | None:
    """Merge the required entries into the lines of an existing .gitignore.

    - existing: the lines of the current file (see SplitLines) or None if there is no file.
    - templateEntries: the lines of the package type's gitignore template, '##PROJECT_NAME##' already replaced.
    - generatorEntries: the file names the active generator writes into the package directory (unordered).

    A legacy unanchored generator entry 'name' is replaced in place by '/name', or dropped when '/name' is already present.
    - An existing file that is sorted and free of duplicates stays that way: the missing entries are inserted at their sorted position
      and empty lines and '/' lines are removed, exactly like the writer always did.
    - Any other existing file keeps its lines exactly (order, comments, blank lines, negations and duplicates). The missing entries are
      appended at the end, after the trailing blank lines are removed.

    Returns the new lines, or None when nothing had to change (the file must not be written then). A new file gets the required entries
    in the writer's sorted form, so later runs keep it sorted. When the template contains a negation or a comment the order matters, so the
    template entries are then kept in template order, followed by the generator entries sorted.
    """
    normalizedGeneratorEntries = _NormalizeGeneratorEntries(generatorEntries)
    requiredEntries = GetRequiredEntries(templateEntries, normalizedGeneratorEntries)
    if existing is None:
        if any(entry.startswith(("!", "#")) for entry in requiredEntries):
            return requiredEntries
        return sorted(requiredEntries)

    lines = [line.replace("\r", "") for line in existing]
    # Decided before anything changes. A sorted file with a negation is kept sorted too: the negation comes before the pattern it
    # belongs to and has no effect, but the file has been like that since the writer sorted it, sorting it again changes nothing.
    keepSorted = IsSortedAndUnique(lines)

    # The generator entries used to be written without the leading '/', those lines are anchored now
    lines, changed = _AnchorLegacyEntries(lines, {entry for entry in normalizedGeneratorEntries if not entry.startswith("/")})

    present = {line.strip() for line in lines}
    missingEntries = [entry for entry in requiredEntries if entry.strip() not in present]
    if len(missingEntries) == 0 and not changed:
        return None

    if keepSorted:
        return sorted({line for line in lines + missingEntries if len(line) > 0 and line != "/"})

    if len(missingEntries) > 0:
        while len(lines) > 0 and _IsBlank(lines[-1]):
            lines.pop()
    return lines + missingEntries
