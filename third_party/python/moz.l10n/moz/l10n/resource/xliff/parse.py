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

from collections import defaultdict
from collections.abc import Iterator
from re import compile
from typing import List, Union, cast

from lxml import etree

from ...message import Expression, Message, PatternMessage
from ..data import Comment, Entry, Metadata, Resource, Section
from ..format import Format
from .common import attrib_as_metadata, element_as_metadata, pretty_name, xliff_ns
from .parse_trans_unit import parse_trans_unit
from .parse_xcode import parse_xliff_stringsdict


def xliff_parse(source: str | bytes) -> Resource[Message, str]:
    """
    Parse an XLIFF 1.2 file into a message resource.

    Sections identify files and groups within them,
    with the first identifier part parsed as the <file> "original" attribute,
    and later parts as <group> "id" attributes.

    An entry's value represents the <target> of a <trans-unit>,
    and its comment the first <note>.
    Other elements and attributes are represented by metadata.

    Metadata keys encode XML element data, using XPath expressions as keys.
    """
    root = etree.fromstring(source.encode() if isinstance(source, str) else source)
    version = root.attrib.get("version", None)
    if version not in ("1.0", "1.1", "1.2"):
        raise ValueError(f"Unsupported <xliff> version: {version}")
    ns = root.nsmap.get(None, "")
    if ns:
        if ns in xliff_ns:
            ns = f"{{{ns}}}"
        else:
            raise ValueError(f"Unsupported namespace: {ns}")

    if root.tag != f"{ns}xliff":
        raise ValueError(f"Unsupported root node: {root}")
    if root.text and not root.text.isspace():
        raise ValueError(f"Unexpected text in <xliff>: {root.text}")

    res: Resource[Message, str] = Resource(Format.xliff, [])
    root_comments = [
        c.text for c in root.itersiblings(etree.Comment, preceding=True) if c.text
    ]
    if root_comments:
        root_comments.reverse()
        res.comment = comment_str(root_comments)
    res.meta = attrib_as_metadata(root)
    for key, uri in root.nsmap.items():
        res.meta.append(Metadata(f"@xmlns:{key}" if key else "@xmlns", uri))

    comment: list[str] = []
    for file in root:
        if file.tail and not file.tail.isspace():
            raise ValueError(f"Unexpected text in <xliff>: {file.tail}")
        if isinstance(file, etree._Comment):
            comment.append(file.text)
        elif file.tag == f"{ns}file":
            file_name = file.attrib.get("original", None)
            if file_name is None:
                raise ValueError(f'Missing "original" attribute for <file>: {file}')
            meta = attrib_as_metadata(file, None, ("original",))
            entries: list[Entry[Message, str] | Comment] = []
            body = None
            for child in file:
                if isinstance(child, etree._Comment):
                    entries.append(Comment(comment_str(child.text)))
                elif child.tag == f"{ns}header":
                    meta += element_as_metadata(child, "header", True)
                elif child.tag == f"{ns}body":
                    if body:
                        raise ValueError(f"Duplicate <body> in <file>: {file}")
                    body = child
                else:
                    raise ValueError(
                        f"Unsupported <{child.tag}> element in <file>: {file}"
                    )
                if child.tail and not child.tail.isspace():
                    raise ValueError(f"Unexpected text in <file>: {child.tail}")

            section = Section((file_name,), entries, meta=meta)
            if comment:
                section.comment = comment_str(comment)
                comment.clear()
            res.sections.append(section)

            if body is None:
                raise ValueError(f"Missing <body> in <file>: {file}")
            elif body.text and not body.text.isspace():
                raise ValueError(f"Unexpected text in <body>: {body.text}")

            if file_name.endswith(".stringsdict"):
                plural_entries = parse_xliff_stringsdict(ns, body)
                if plural_entries is not None:
                    entries += cast(
                        List[Union[Entry[Message, str], Comment]], plural_entries
                    )
                    continue

            for unit in body:
                if isinstance(unit, etree._Comment):
                    entries.append(Comment(comment_str(unit.text)))
                elif unit.tag == f"{ns}trans-unit":
                    entries.append(parse_trans_unit(unit))
                elif unit.tag == f"{ns}bin-unit":
                    entries.append(parse_bin_unit(unit))
                elif unit.tag == f"{ns}group":
                    res.sections += parse_group(ns, [file_name], unit)
                else:
                    raise ValueError(
                        f"Unsupported <{unit.tag}> element in <body>: {body}"
                    )
                if unit.tail and not unit.tail.isspace():
                    raise ValueError(f"Unexpected text in <body>: {unit.tail}")
    return res


def parse_group(
    ns: str, parent: list[str], group: etree._Element
) -> Iterator[Section[Message, str]]:
    id = group.attrib.get("id", "")
    path = [*parent, id]
    meta = attrib_as_metadata(group, None, ("id",))
    entries: list[Entry[Message, str] | Comment] = []
    if group.text and not group.text.isspace():
        raise ValueError(f"Unexpected text in <group>: {group.text}")

    # Note that this is modified after being emitted,
    # To ensure that nested groups are ordered by path
    yield Section(tuple(path), entries, meta=meta)

    seen: dict[str, int] = defaultdict(int)
    for unit in group:
        if isinstance(unit, etree._Comment):
            entries.append(Comment(comment_str(unit.text)))
        elif unit.tag == f"{ns}trans-unit":
            entries.append(parse_trans_unit(unit))
        elif unit.tag == f"{ns}bin-unit":
            entries.append(parse_bin_unit(unit))
        elif unit.tag == f"{ns}group":
            yield from parse_group(ns, path, unit)
        else:
            name = pretty_name(unit, unit.tag)
            idx = seen[name] + 1
            unit_base = f"{name}[{idx}]" if idx > 1 else name
            meta += element_as_metadata(unit, unit_base, True)
            seen[name] = idx
        if unit.tail and not unit.tail.isspace():
            raise ValueError(f"Unexpected text in <group>: {unit.tail}")


def parse_bin_unit(unit: etree._Element) -> Entry[Message, str]:
    id = unit.attrib.get("id", None)
    if id is None:
        raise ValueError(f'Missing "id" attribute for <bin-unit>: {unit}')
    meta = attrib_as_metadata(unit, None, ("id",))
    meta += element_as_metadata(unit, "", False)
    msg = PatternMessage([Expression(None, attributes={"bin-unit": None})])
    return Entry((id,), msg, meta=meta)


dash_indent = compile(r" .+(\n   - .*)+ ")


def comment_str(body: list[str] | str) -> str:
    if isinstance(body, str):
        body = [body]
    lines: list[str] = []
    for comment in body:
        if comment:
            if dash_indent.fullmatch(comment):
                # A dash is considered as a part of the indent if it's aligned
                # with the last dash of <!-- in a top-level comment.
                lines.append(comment.replace("\n   - ", "\n").strip(" "))
            else:
                lines.append(
                    "\n".join(line.strip() for line in comment.splitlines()).strip("\n")
                )
    return "\n\n".join(lines).strip("\n")
