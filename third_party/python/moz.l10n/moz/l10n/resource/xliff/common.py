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

from lxml.etree import QName, _Comment, _Element

from ..data import Metadata

xliff_ns = {
    "urn:oasis:names:tc:xliff:document:1.0",
    "urn:oasis:names:tc:xliff:document:1.1",
    "urn:oasis:names:tc:xliff:document:1.2",
}
xml_ns = "http://www.w3.org/XML/1998/namespace"


def element_as_metadata(
    el: _Element, base: str, with_attrib: bool
) -> Iterator[Metadata[str]]:
    is_empty = True
    seen: dict[str, int] = defaultdict(int)
    if with_attrib:
        am = attrib_as_metadata(el, base)
        if am:
            yield from am
            is_empty = False
    if el.text and not el.text.isspace():
        yield Metadata(base, el.text)
        is_empty = False
    for child in el:
        if isinstance(child, _Comment):
            yield Metadata(f"{base}/comment()" if base else "comment()", child.text)
        elif isinstance(child.tag, str):
            name = pretty_name(child, child.tag)
            idx = seen[name] + 1
            child_base = f"{base}/{name}" if base else name
            if idx > 1:
                child_base += f"[{idx}]"
            yield from element_as_metadata(child, child_base, True)
            seen[name] = idx
        else:
            raise ValueError(f"Unsupported metadata element in {base}: {el}")
        if child.tail and not child.tail.isspace():
            yield Metadata(base, child.tail)
        is_empty = False
    if is_empty and with_attrib:
        yield Metadata(base, "")


def attrib_as_metadata(
    el: _Element, base: str | None = None, exclude: tuple[str] | None = None
) -> list[Metadata[str]]:
    res = []
    for key, value in el.attrib.items():
        if not exclude or key not in exclude:
            name = pretty_name(el, key)
            pk = f"{base}/@{name}" if base else f"@{name}"
            res.append(Metadata(pk, value))
    return res


def pretty_name(el: _Element, name: str) -> str:
    if not name.startswith("{"):
        return name
    q = QName(name)
    ns = q.namespace
    if not ns or ns in xliff_ns:
        return q.localname
    if ns == xml_ns:
        return f"xml:{q.localname}"
    ns_key = next(iter(k for k, v in el.nsmap.items() if v == ns), None)
    if ns_key:
        return f"{ns_key}:{q.localname}"
    else:
        raise ValueError(f"Name with unknown namespace: {name}")


def clark_name(nsmap: dict[str | None, str], name: str) -> str:
    """See https://lxml.de/tutorial.html#namespaces"""
    if ":" not in name:
        return name
    ns, local = name.split(":", 1)
    if ns in nsmap:
        return QName(nsmap[ns], local).text
    if ns == "xml":
        return f"{{{xml_ns}}}{local}"
    raise ValueError(f"Name with unknown namespace: {name}")
