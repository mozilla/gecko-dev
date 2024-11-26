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

from lxml import etree

from ...message import Markup, Message, PatternMessage, VariableRef
from ..data import Entry, Metadata
from .common import attrib_as_metadata, element_as_metadata, pretty_name, xliff_ns


def parse_trans_unit(unit: etree._Element) -> Entry[Message, str]:
    id = unit.attrib.get("id", None)
    if id is None:
        raise ValueError(f'Missing "id" attribute for <trans-unit>: {unit}')
    meta = attrib_as_metadata(unit, None, ("id",))
    if unit.text and not unit.text.isspace():
        raise ValueError(f"Unexpected text in <trans-unit>: {unit.text}")

    target = None
    note = None
    seen: dict[str, int] = defaultdict(int)
    for el in unit:
        if isinstance(el, etree._Comment):
            meta.append(Metadata("comment()", el.text))
        else:
            name = pretty_name(el, el.tag)
            if name == "target":
                if target:
                    raise ValueError(f"Duplicate <target> in <trans-unit> {id}: {unit}")
                target = el
                meta += attrib_as_metadata(el, "target")
            elif name == "note" and note is None and el.text:
                note = el
                note_attrib = attrib_as_metadata(el, "note")
                if note_attrib:
                    meta += note_attrib
                elif el != unit[-1]:
                    # If there are elements after this <note>,
                    # add a marker for its relative position.
                    meta.append(Metadata("note", ""))
                seen[name] += 1
            else:
                idx = seen[name] + 1
                base = f"{name}[{idx}]" if idx > 1 else name
                meta += element_as_metadata(el, base, True)
                seen[name] = idx
        if el.tail and not el.tail.isspace():
            raise ValueError(f"Unexpected text in <trans-unit>: {el.tail}")

    comment = "" if note is None else note.text or ""
    msg = PatternMessage([] if target is None else list(parse_pattern(target)))
    return Entry((id,), msg, comment, meta)


def parse_pattern(el: etree._Element) -> Iterator[str | Markup]:
    if el.text:
        yield el.text
    for child in el:
        q = etree.QName(child.tag)
        ns = q.namespace
        name = q.localname if not ns or ns in xliff_ns else q.text
        options: dict[str, str | VariableRef] = dict(child.attrib)
        if name in ("x", "bx", "ex"):
            yield Markup("standalone", name, options)
        elif isinstance(child.tag, str):
            yield Markup("open", name, options)
            yield from parse_pattern(child)
            yield Markup("close", name)
        if child.tail:
            yield child.tail
