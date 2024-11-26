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

from re import Match, compile
from typing import Any, Callable, Iterator, Literal

from moz.l10n.message import Message, PatternMessage

from ..data import Entry, Resource

control_chars = compile(r"[\x00-\x19\x5C\x7F-\x9F]")
not_ascii_printable_chars = compile(r"[^\x20-\x5B\x5D-\x7E]")
special_key_trans = str.maketrans({" ": "\\ ", ":": "\\:", "=": "\\="})


def encode_char(m: Match[str]) -> str:
    ch = m.group()
    if ch == "\\":
        return r"\\"
    elif ch == "\t":
        return r"\t"
    elif ch == "\n":
        return r"\n"
    elif ch == "\f":
        return r"\f"
    elif ch == "\r":
        return r"\r"
    return f"\\u{ord(ch):04x}"


def properties_serialize(
    resource: Resource[str, Any] | Resource[Message, Any],
    encoding: Literal["iso-8859-1", "utf-8", "utf-16"] = "utf-8",
    serialize_message: Callable[[Message], str] | None = None,
    trim_comments: bool = False,
) -> Iterator[str]:
    """
    Serialize a resource as the contents of a .properties file.

    Section identifiers will be prepended to their constituent message identifiers.
    Multi-part message identifiers will be joined with `.` between each part.

    For non-string message values, a `serialize_message` callable must be provided.

    Metadata is not supported.

    Yields each entry, comment, and empty line separately.
    Re-parsing a serialized .properties file is not guaranteed to result in the same Resource,
    as the serialization may lose information about message sections and metadata.
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
        yield from comment(section.comment, section.meta, True)
        id_prefix = ".".join(section.id) + "." if section.id else ""
        for entry in section.entries:
            if isinstance(entry, Entry):
                yield from comment(entry.comment, entry.meta, False)
                key = id_prefix + ".".join(entry.id)
                key = (
                    control_chars.sub(encode_char, key)
                    if encoding in {"utf-8", "utf-16"}
                    else not_ascii_printable_chars.sub(encode_char, key)
                )
                key = key.translate(special_key_trans)

                value: str
                msg = entry.value
                if isinstance(msg, str):
                    value = msg
                elif serialize_message:
                    value = serialize_message(msg)
                elif isinstance(msg, PatternMessage) and all(
                    isinstance(p, str) for p in msg.pattern
                ):
                    value = "".join(msg.pattern)  # type: ignore[arg-type]
                else:
                    raise ValueError(f"Unsupported message for {key}: {msg}")

                value = (
                    control_chars.sub(encode_char, value)
                    if encoding in {"utf-8", "utf-16"}
                    else not_ascii_printable_chars.sub(encode_char, value)
                )

                if value[0:1].isspace():
                    value = "\\" + value
                if value.endswith(" ") and not value.endswith("\\ "):
                    value = value[:-1] + "\\u0020"
                yield f"{key} = {value}\n" if value else f"{key} =\n"
                at_empty_line = False
            else:
                yield from comment(entry.comment, None, True)
