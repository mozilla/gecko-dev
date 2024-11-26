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
from json import dumps
from re import sub
from typing import Any

from ...message import Declaration, Expression, Message, PatternMessage, VariableRef
from ..data import Entry, Resource


def webext_serialize(
    resource: Resource[str, Any] | Resource[Message, Any],
    trim_comments: bool = False,
) -> Iterator[str]:
    """
    Serialize a resource as the contents of a messages.json file.

    Section identifiers and multi-part message identifiers are not supported.
    Resource and section comments are not supported.
    Metadata is not supported.

    Yields the entire JSON result as a single string.
    """

    def check(comment: str | None, meta: Any) -> None:
        if trim_comments:
            return
        if comment:
            raise ValueError("Resource and section comments are not supported")
        if meta:
            raise ValueError("Metadata is not supported")

    check(resource.comment, resource.meta)
    res: dict[str, Any] = {}
    for section in resource.sections:
        if section.id:
            raise ValueError(f"Section identifiers not supported: {section.id}")
        check(section.comment, section.meta)
        for entry in section.entries:
            if isinstance(entry, Entry):
                check(None, entry.meta)
                if len(entry.id) != 1:
                    raise ValueError(f"Unsupported entry identifier: {entry.id}")
                name = entry.id[0]
                if isinstance(entry.value, str):
                    res[name] = {"message": sub(r"\$+", r"$\g<0>", entry.value)}
                    if not trim_comments and entry.comment:
                        res[name]["description"] = entry.comment
                elif isinstance(entry.value, PatternMessage):
                    res[name] = webext_message(name, entry, trim_comments)  # type: ignore[arg-type]
                else:
                    raise ValueError(f"Unsupported entry for {name}: {entry.value}")
            else:
                check(entry.comment, None)
    yield dumps(res, indent=2)
    yield "\n"


def webext_message(
    name: str, entry: Entry[PatternMessage, Any], trim_comments: bool
) -> dict[str, Any]:
    msg = ""
    placeholders: dict[str, Any] = {}
    for part in entry.value.pattern:
        if isinstance(part, str):
            msg += sub(r"\$+", r"$\g<0>", part)
        elif (
            isinstance(part, Expression)
            and isinstance(part.arg, VariableRef)
            and part.annotation is None
        ):
            ph_name = part.arg.name
            source = part.attributes.get("source", None)
            local = next(
                (
                    d.value
                    for d in entry.value.declarations
                    if isinstance(d, Declaration) and d.name == ph_name
                ),
                None,
            )
            if local:
                local_source = local.attributes.get("source", None)
                if isinstance(local_source, str):
                    content = local_source
                elif isinstance(local.arg, VariableRef):
                    content = local.arg.name
                    if not content.startswith("$"):
                        content = f"${content}"
                elif isinstance(local.arg, str):
                    content = local.arg
                else:
                    raise ValueError(
                        f"Unsupported placeholder for {ph_name} in {name}: {local}"
                    )
                if local.annotation:
                    raise ValueError(
                        f"Unsupported annotation for {ph_name} in {name}: {local}"
                    )
                if (
                    isinstance(source, str)
                    and len(source) >= 3
                    and source.startswith("$")
                    and source.endswith("$")
                ):
                    ph_name = source[1:-1]
                else:
                    source = None
                if not any(key.lower() == ph_name.lower() for key in placeholders):
                    placeholders[ph_name] = {"content": content}
                    example = (
                        None if trim_comments else local.attributes.get("example", None)
                    )
                    if isinstance(example, str):
                        placeholders[ph_name]["example"] = example
                    elif example:
                        raise ValueError(
                            f"Unsupported placeholder example for {ph_name} in {name}: {example}"
                        )
                msg += source or f"${ph_name}$"
            else:
                arg_name = source if isinstance(source, str) else ph_name
                msg += arg_name if arg_name.startswith("$") else f"${arg_name}"
        else:
            raise ValueError(f"Unsupported message part for {name}: {part}")
    res: dict[str, Any] = {"message": msg}
    if not trim_comments and entry.comment:
        res["description"] = entry.comment
    if placeholders:
        res["placeholders"] = placeholders
    return res
