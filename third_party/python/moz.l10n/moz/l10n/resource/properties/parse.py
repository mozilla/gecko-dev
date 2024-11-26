# Copyright Mozilla Foundation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from __future__ import annotations

from enum import Enum
from re import Match, compile
from typing import Any, Callable

from moz.l10n.message import Message, PatternMessage

from ..data import Comment, Entry, LinePos, Resource, Section
from ..format import Format


class LineKind(Enum):
    EMPTY = 0
    COMMENT = 1
    KEY = 2
    VALUE = 3


esc_re = compile("\\\\(u[0-9A-Fa-f]{1,4}|.)")


def esc_parse(match: Match[str]) -> str:
    esc = match.group(1)
    if len(esc) > 1 and esc[0] == "u":
        n = int(esc[1:], 16)
        return chr(n)
    elif esc == "t":
        return "\t"
    elif esc == "n":
        return "\n"
    elif esc == "f":
        return "\f"
    elif esc == "r":
        return "\r"
    else:
        return esc


def properties_parse(
    source: bytes | str,
    encoding: str = "utf-8",
    parse_message: Callable[[str], Message] | None = None,
) -> Resource[Message, Any]:
    """
    Parse a .properties file into a message resource.

    By default, all messages are parsed as PatternMessage([str]).
    To customize that, define an appropriate `parse_message(str) -> Message`.

    The parsed resource will not include any metadata.
    """
    if not isinstance(source, str):
        source = source.decode(encoding)
    parser = PropertiesParser(source)
    entries: list[Entry[Message, Any] | Comment] = []
    resource = Resource(Format.properties, [Section((), entries)])

    start_line = 0
    comment = ""
    prev_linepos: LinePos | None = None
    entry: Entry[Message, Any] | None = None
    for kind, line, value in parser:
        if kind == LineKind.VALUE:
            assert entry
            if parse_message:
                entry.value = parse_message(value)
            else:
                assert isinstance(entry.value, PatternMessage)
                entry.value.pattern.append(value)
            if entry.linepos and line > entry.linepos.key:
                entry.linepos.value = line
                entry.linepos.end = line + 1
            entry = None
        else:
            if prev_linepos:
                prev_linepos.end = max(prev_linepos.end, line)
                prev_linepos = None
            entry = None
            if kind == LineKind.KEY:
                entry = Entry(
                    id=(value,),
                    value=PatternMessage([]),
                    comment=comment,
                    linepos=LinePos(start_line or line, line, line, line + 1),
                )
                prev_linepos = entry.linepos
                entries.append(entry)
                comment = ""
                start_line = 0
            elif kind == LineKind.COMMENT:
                if comment:
                    comment += "\n" + value
                else:
                    comment = value
                    start_line = line
            elif comment:
                # empty line or EOF after a comment
                if entries or resource.comment:
                    entries.append(Comment(comment))
                else:
                    resource.comment = comment
                comment = ""
                start_line = 0
    return resource


class PropertiesParser:
    def __init__(self, source: str) -> None:
        self.source = source
        self.pos = 0
        self.line_pos = 1
        self.at_value = False
        self.done = False

    def __iter__(self) -> PropertiesParser:
        return self

    def __next__(self) -> tuple[LineKind, int, str]:
        if self.done:
            raise StopIteration

        lp = self.line_pos
        self.ws()
        if self.pos == len(self.source):
            self.done = True
            return LineKind.EMPTY, lp, ""

        if self.nl():
            self.at_value = False
            return LineKind.EMPTY, lp, ""

        if self.at_value:
            # value
            self.at_value = False
            line_start = start = self.pos
            at_escape = False
            at_cr = False
            idx = -1
            lines: list[str] = []
            for idx, ch in enumerate(self.source[start:]):
                if ch == "\n" or ch == "\r":
                    if at_escape:
                        at_escape = False
                        self.line_pos += 1
                        end = start + idx - 1
                        lines.append(self.source[line_start:end])
                        at_cr = ch == "\r"
                        line_start = start + idx + 1
                    elif ch == "\n" and at_cr:
                        at_cr = False
                        line_start = start + idx + 1
                    else:
                        idx -= 1
                        break
                else:
                    if at_cr:
                        at_cr = False
                    if at_escape:
                        at_escape = False
                    elif ch == "\\":
                        at_escape = True
            self.pos = end = start + idx + 1
            lines.append(self.source[line_start:end])
            self.nl()
            value = "".join(
                esc_re.sub(esc_parse, line.lstrip("\f\t ")) for line in lines
            )
            return LineKind.VALUE, lp, value

        if self.source.startswith(("#", "!"), self.pos):
            # comment
            self.pos += 1
            if self.source.startswith(" ", self.pos):
                # Ignore one space after #, if present.
                self.pos += 1
            start = self.pos
            idx = -1
            for idx, ch in enumerate(self.source[start:]):
                if ch == "\n" or ch == "\r":
                    idx -= 1
                    break
            end = self.pos = start + idx + 1
            self.nl()
            return LineKind.COMMENT, lp, self.source[start:end]

        # key
        start = self.pos
        at_escape = False
        idx = -1
        for idx, ch in enumerate(self.source[start:]):
            if at_escape:
                at_escape = False
            elif ch == "\\":
                at_escape = True
            elif ch in {"\n", "\r", "\t", "\f", " ", "=", ":"}:
                idx -= 1
                break
        end = self.pos = start + idx + 1
        self.ws()
        if self.source.startswith(("=", ":"), self.pos):
            self.pos += 1
        self.at_value = True
        return LineKind.KEY, lp, esc_re.sub(esc_parse, self.source[start:end])

    def nl(self) -> bool:
        if self.source.startswith("\n", self.pos):
            self.pos += 1
            self.line_pos += 1
            return True
        elif self.source.startswith("\r", self.pos):
            self.pos += 1
            self.line_pos += 1
            if self.source.startswith("\n", self.pos):
                self.pos += 1
            return True
        return False

    def ws(self) -> None:
        for ch in self.source[self.pos :]:
            if ch in {" ", "\t", "\f"}:
                self.pos += 1
            else:
                break
