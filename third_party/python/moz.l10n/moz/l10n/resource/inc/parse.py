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

from re import compile
from typing import Any

from moz.l10n.message import Message, PatternMessage

from ..data import Comment, Entry, Resource, Section
from ..format import Format

re_define = compile(r"#define[ \t]+(\w+)(?:[ \t](.*))?")


def inc_parse(source: str | bytes) -> Resource[Message, Any]:
    """
    Parse a .inc file into a message resource.

    Directives such as `#filter` and `#unfilter` will be stored as standalone comments.

    The parsed resource will not include any metadata.
    """
    entries: list[Entry[Message, Any] | Comment] = []
    comment: str = ""
    if not isinstance(source, str):
        source = source.decode()
    for line in source.splitlines():
        if not line or line.isspace():
            if comment:
                entries.append(Comment(comment))
                comment = ""
        elif line.startswith("# "):
            nc = line[2:].lstrip()
            if nc.startswith("#"):
                nc = line
            comment = f"{comment}\n{nc}" if comment else nc
        else:
            match = re_define.fullmatch(line)
            if match:
                name, value = match.groups()
                entries.append(Entry((name,), PatternMessage([value]), comment))
                comment = ""
            elif line.startswith("#"):
                if comment:
                    entries.append(Comment(comment))
                    comment = ""
                entries.append(Comment(line))
            else:
                raise ValueError(f"Unsupported content: {line}")
    if comment:
        entries.append(Comment(comment))
    return Resource(Format.inc, [Section((), entries)])
