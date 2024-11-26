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

from polib import POEntry, POFile

from ...message import FunctionAnnotation, Message, PatternMessage, SelectMessage
from ..data import Entry, Resource


def po_serialize(
    resource: Resource[str, str] | Resource[Message, str],
    trim_comments: bool = False,
    wrapwidth: int = 78,
) -> Iterator[str]:
    """
    Serialize a resource as the contents of a .po file.

    Multi-part identifiers will be joined with `.` between each part.
    Section identifiers are serialized as message contexts.
    Comments and metadata on sections is not supported.

    Yields each entry and empty line separately.
    Re-parsing a serialized .properties file is not guaranteed to result in the same Resource,
    as the serialization may lose information about message identifiers.
    """

    pf = POFile(wrapwidth=wrapwidth)
    if not trim_comments:
        pf.header = resource.comment.rstrip() + "\n"
    pf.metadata = {m.key: str(m.value) for m in resource.meta}
    yield str(pf)

    for section in resource.sections:
        if section.comment:
            raise ValueError(f"Section comments are not supported: {section.id}")
        if section.meta:
            raise ValueError(f"Section metadata is not supported: {section.id}")
        context = ".".join(section.id) if section.id else None
        for entry in section.entries:
            if isinstance(entry, Entry):
                pe = POEntry(msgctxt=context, msgid=".".join(entry.id))
                msg = entry.value
                if isinstance(msg, str):
                    pe.msgstr = msg
                elif isinstance(msg, PatternMessage) and all(
                    isinstance(p, str) for p in msg.pattern
                ):
                    pe.msgstr = "".join(msg.pattern)  # type: ignore[arg-type]
                elif (
                    isinstance(msg, SelectMessage)
                    and not msg.declarations
                    and len(msg.selectors) == 1
                    and isinstance(msg.selectors[0].annotation, FunctionAnnotation)
                    and msg.selectors[0].annotation.name == "number"
                    and not msg.selectors[0].annotation.options
                    and all(
                        len(keys) == 1 and all(isinstance(p, str) for p in pattern)
                        for keys, pattern in msg.variants.items()
                    )
                ):
                    values = [
                        "".join(pattern)  # type: ignore[arg-type]
                        for pattern in msg.variants.values()
                    ]
                    if len(values) == 1:
                        pe.msgstr = values[0]
                    else:
                        pe.msgstr_plural = {idx: str for idx, str in enumerate(values)}
                else:
                    raise ValueError(
                        f"Value for {entry.id} is not supported: {entry.value}"
                    )
                if not trim_comments:
                    pe.tcomment = entry.comment.rstrip()
                for m in entry.meta:
                    if m.key == "obsolete":
                        pe.obsolete = m.value != "false"
                    elif m.key == "plural":
                        pe.msgid_plural = m.value
                    elif not trim_comments:
                        if m.key == "translator-comments":
                            cs = (m.value).lstrip("\n").rstrip()
                            pe.tcomment = f"{pe.tcomment}\n{cs}" if pe.tcomment else cs
                        elif m.key == "extracted-comments":
                            pe.comment = (m.value).lstrip("\n").rstrip()
                        elif m.key == "reference":
                            pos = m.value.split(":", 1)
                            pe.occurrences.append(
                                (pos[0], pos[1]) if len(pos) == 2 else (m.value, "")
                            )
                        elif m.key == "flag":
                            pe.flags.append(m.value)
                        else:
                            raise ValueError(
                                f'Unsupported meta entry "{m.key}" for {entry.id}: {m.value}'
                            )
                if not pe.obsolete or not trim_comments:
                    yield "\n"
                    yield pe.__unicode__(wrapwidth=wrapwidth)
            else:
                raise ValueError(
                    f"Standalone comments are not supported: {entry.comment}"
                )
