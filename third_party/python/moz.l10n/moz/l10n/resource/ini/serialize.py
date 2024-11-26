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

from collections.abc import Iterator
from re import search
from typing import Any

from moz.l10n.message import Message, PatternMessage

from ..data import Entry, Resource


def ini_serialize(
    resource: Resource[str, Any] | Resource[Message, Any],
    trim_comments: bool = False,
) -> Iterator[str]:
    """
    Serialize a resource as the contents of an .ini file.

    Anonymous sections are not supported.
    Multi-part section and message identifiers will be joined with `.` between each part.

    Metadata is not supported.

    Comment lines not starting with `#` will be separated from their `#` prefix with a space.

    Yields each entry, continuation line, comment, and empty line separately.
    Re-parsing a serialized .ini file is not guaranteed to result in the same Resource,
    as the serialization may lose information about metadata.
    """

    at_empty_line = True

    def comment(comment: str, meta: Any, standalone: bool) -> Iterator[str]:
        nonlocal at_empty_line
        if trim_comments:
            return
        if meta:
            raise ValueError("Metadata is not supported")
        if comment:
            if standalone and not at_empty_line:
                yield "\n"
            for line in comment.strip("\n").split("\n"):
                if not line or line.isspace():
                    yield "#\n"
                else:
                    line = line.rstrip() + "\n"
                    yield f"#{line}" if line.startswith("#") else f"# {line}"
            if standalone:
                yield "\n"
                at_empty_line = True

    yield from comment(resource.comment, resource.meta, True)
    for section in resource.sections:
        if not section.id:
            raise ValueError("Anonymous sections are not supported")
        yield from comment(section.comment, section.meta, False)
        yield f"[{id_str(section.id)}]\n"
        at_empty_line = False
        for entry in section.entries:
            if isinstance(entry, Entry):
                yield from comment(entry.comment, entry.meta, False)
                msg = entry.value
                if isinstance(msg, str):
                    value = msg
                elif isinstance(msg, PatternMessage) and all(
                    isinstance(p, str) for p in msg.pattern
                ):
                    value = "".join(msg.pattern)  # type: ignore[arg-type]
                else:
                    raise ValueError(f"Unsupported message for {entry.id}: {msg}")
                lines = value.rstrip().splitlines()
                yield (
                    f"{id_str(entry.id)}={lines.pop(0) if lines else ''}".rstrip()
                    + "\n"
                )
                for line in lines:
                    ls = line.rstrip()
                    yield f"  {ls}\n" if ls else "\n"
                at_empty_line = False
            else:
                yield from comment(entry.comment, None, True)


def id_str(id: tuple[str, ...]) -> str:
    name = ".".join(id)
    if search(r"^\s|[\n:=[\]]|\s$", name):
        raise ValueError(f"Unsupported character in identifier: {id}")
    return name
