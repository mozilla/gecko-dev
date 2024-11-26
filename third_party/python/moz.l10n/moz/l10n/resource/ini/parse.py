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

from io import StringIO
from typing import Any, Generator, TextIO

from iniparse import ini  # type: ignore[import-untyped]

from moz.l10n.message import Message, PatternMessage

from ..data import Comment, Entry, Resource, Section
from ..format import Format


def ini_parse(source: TextIO | str | bytes) -> Resource[Message, Any]:
    """
    Parse an .ini file into a message resource.

    The parsed resource will not include any metadata.
    """
    if isinstance(source, str):
        file: TextIO = StringIO(source)
    elif isinstance(source, bytes):
        file = StringIO(source.decode())
    else:
        file = source
    cfg = ini.INIConfig(file, optionxformvalue=None)

    resource = Resource[Message, str](Format.ini, [])
    section: Section[Message, str] | None = None
    pattern: list[str] | None = None
    comment = ""

    def add_comment(cl: str | None) -> None:
        nonlocal comment
        if cl is not None:
            cv = cl[1:] if cl and cl.startswith(" ") else cl
            comment = f"{comment}\n{cv}" if comment else cv

    for line in ini_lines(cfg._data):
        if pattern:
            if isinstance(line, ini.ContinuationLine):
                pattern[0] += "\n" + line.value
                continue
            elif isinstance(line, ini.EmptyLine):
                pattern[0] += "\n"
            else:
                pattern[0] = pattern[0].rstrip("\n")
                pattern = None
        if isinstance(line, ini.SectionLine):
            add_comment(line.comment)
            section = Section((line.name,), [], comment)
            comment = ""
            resource.sections.append(section)
        elif isinstance(line, ini.OptionLine):
            if not section:
                raise ValueError(f"Unexpected value {line.name} before section header")
            add_comment(line.comment)
            pattern = [line.value]
            msg = PatternMessage(pattern)  # type: ignore[arg-type]
            section.entries.append(Entry((line.name,), msg, comment))
            comment = ""
        elif isinstance(line, ini.CommentLine):
            add_comment(line.comment)
        elif isinstance(line, ini.EmptyLine):
            if comment:
                if section:
                    section.entries.append(Comment(comment))
                else:
                    resource.comment = (
                        f"{resource.comment}\n\n{comment}"
                        if resource.comment
                        else comment
                    )
                comment = ""
        else:
            raise ValueError(f"Unexpected {line.__class__.__name__}: {line.__dict__}")
    if pattern:
        pattern[0] = pattern[0].rstrip("\n")
    if comment:
        if section:
            section.entries.append(Comment(comment))
        else:
            resource.comment = (
                f"{resource.comment}\n\n{comment}" if resource.comment else comment
            )
    return resource


def ini_lines(data: ini.LineContainer) -> Generator[ini.LineType, None, None]:
    for line in data.contents:
        if isinstance(line, ini.LineContainer):
            yield from ini_lines(line)
        else:
            yield line
