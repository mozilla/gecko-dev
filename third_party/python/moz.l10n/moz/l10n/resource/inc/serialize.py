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
from typing import Any

from moz.l10n.message import Message, PatternMessage

from ..data import Entry, Resource


def inc_serialize(
    resource: Resource[str, Any] | Resource[Message, Any],
    trim_comments: bool = False,
) -> Iterator[str]:
    """
    Serialize a resource as the contents of a .inc file.

    Section identifiers and multi-part identifiers are not supported.

    Non-string message values and metadata is not supported.

    Comment lines not starting with `#` will be prefixed with `# `.

    Yields each line separately.
    """

    def comment(comment: str, meta: Any, standalone: bool) -> Iterator[str]:
        if meta:
            raise ValueError("Metadata is not supported")
        if comment:
            for line in comment.strip("\n").split("\n"):
                if line.startswith("#"):
                    if not trim_comments or not line.startswith("# "):
                        yield f"{line}\n"
                elif not trim_comments:
                    line = line.strip()
                    yield f"# {line}\n" if line else "#\n"
            if standalone:
                yield "\n"

    yield from comment(resource.comment, resource.meta, True)
    for section in resource.sections:
        yield from comment(section.comment, section.meta, True)
        if section.id:
            raise ValueError(f"Section identifiers not supported: {section.id}")
        for entry in section.entries:
            if isinstance(entry, Entry):
                yield from comment(entry.comment, entry.meta, False)
                if len(entry.id) != 1:
                    raise ValueError(f"Unsupported identifier: {entry.id}")
                msg = entry.value
                if isinstance(msg, str):
                    value = msg
                elif isinstance(msg, PatternMessage) and all(
                    isinstance(p, str) for p in msg.pattern
                ):
                    value = "".join(msg.pattern)  # type: ignore[arg-type]
                else:
                    raise ValueError(f"Unsupported message for {entry.id[0]}: {msg}")
                value = value.replace("\n", " ")
                yield f"#define {entry.id[0]} {value}\n"
                yield "\n"
            else:
                yield from comment(entry.comment, None, True)
