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
from dataclasses import dataclass
from re import compile
from typing import NoReturn

from lxml import etree

from ...message import (
    CatchallKey,
    Expression,
    FunctionAnnotation,
    SelectMessage,
    VariableRef,
    Variants,
)
from ..data import Entry, Metadata
from .common import attrib_as_metadata


@dataclass
class XcodePluralElements:
    unit: etree._Element
    source: etree._Element
    target: etree._Element | None


@dataclass
class XcodePlural:
    var_name: str
    format_key: XcodePluralElements | None
    variants: dict[str, XcodePluralElements]


plural_categories = ("zero", "one", "two", "few", "many", "other")
variant_key = compile(r"%#@([a-zA-Z_]\w*)@")
# https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Strings/Articles/formatSpecifiers.html
printf = compile(
    r"%([1-9]\$)?[-#+ 0,]?[0-9.]*(?:(?:hh?|ll?|qztj)[douxX]|L[aAeEfFgG]|[@%aAcCdDeEfFgGoOspSuUxX])"
)


def parse_xliff_stringsdict(
    ns: str, body: etree._Element
) -> list[Entry[SelectMessage, str]] | None:
    plurals: dict[str, XcodePlural] = {}
    for unit in body:
        if unit.tag != f"{ns}trans-unit":
            return None
        if unit.text and not unit.text.isspace():
            raise ValueError(f"Unexpected text in <trans-unit>: {unit.text}")
        id = unit.attrib.get("id", None)
        if id and id.startswith("/") and id.endswith(":dict/:string"):
            # If we get this far, this is clearly trying to be an Xcode plural.
            # Therefore, treat any further deviations as errors.
            parse_xliff_stringsdict_unit(ns, plurals, unit)
        else:
            return None

    entries = []
    for msg_id, plural in plurals.items():
        selector = Expression(
            VariableRef(plural.var_name),
            FunctionAnnotation("number"),
            {"source": plural.format_key.source.text if plural.format_key else None},
        )
        meta: list[Metadata[str]] = []
        if plural.format_key:
            meta += attrib_as_metadata(plural.format_key.unit, "format", ("id",))
            if plural.format_key.target is not None:
                meta += attrib_as_metadata(plural.format_key.target, "format/target")
        variants: Variants = {}
        for key, variant in plural.variants.items():
            meta += attrib_as_metadata(variant.unit, key, ("id",))
            meta.append(Metadata(f"{key}/source", variant.source.text or ""))
            if variant.target is None:
                pattern_src = None
            else:
                meta += attrib_as_metadata(variant.target, f"{key}/target")
                pattern_src = variant.target.text
            variants[(CatchallKey("other") if key == "other" else key,)] = list(
                parse_pattern(pattern_src)
            )
        entries.append(Entry((msg_id,), SelectMessage([selector], variants), meta=meta))
    return entries


def parse_xliff_stringsdict_unit(
    ns: str, plurals: dict[str, XcodePlural], unit: etree._Element
) -> None:
    id_parts = unit.attrib["id"].split(":dict/")
    msg_id = id_parts[0][1:]

    def error(message: str) -> NoReturn:
        raise ValueError(f"{message} in Xcode plural definition {unit.attrib['id']}")

    source = None
    target = None
    for el in unit:
        if len(el) > 0:
            error(f"Unexpected child elements of <{el.tag}>")
        if el.tag == f"{ns}source":
            if el.attrib:
                error("Unexpected attributes of <source>")
            if source is None:
                source = el
            else:
                error("Duplicate <source>")
        elif el.tag == f"{ns}target":
            if target is None:
                target = el
            else:
                error("Duplicate <target>")
        elif el.tag == f"{ns}note":
            if el.attrib or el.text:
                error("Unexpected not-empty <note>")
        else:
            error(f"Unexpected <{el.tag}>")
        if el.tail and not el.tail.isspace():
            raise ValueError(f"Unexpected text in <trans-unit>: {el.tail}")
    if source is None:
        error("Missing <source>")
    var_data = XcodePluralElements(unit, source, target)

    if id_parts[1] == "NSStringLocalizedFormatKey":
        if len(id_parts) != 3:
            error("Unexpected Xcode plurals id")
        var_match = variant_key.search(source.text or "")
        if var_match is None:
            error("Unexpected <source> value")
        if msg_id in plurals:
            prev = plurals[msg_id]
            if prev.format_key is None:
                prev.format_key = var_data
            else:
                error("Duplicate NSStringLocalizedFormatKey")
            if var_match[1] != prev.var_name:
                error("Mismatching key values")
        else:
            plurals[msg_id] = XcodePlural(var_match[1], var_data, {})
    else:
        if len(id_parts) != 4:
            error("Unexpected Xcode plurals id")
        var_name = id_parts[1]
        plural_cat = id_parts[2]
        if plural_cat not in plural_categories:
            error("Invalid plural category")
        if msg_id in plurals:
            prev = plurals[msg_id]
            if var_name != prev.var_name:
                error("Mismatching key values")
            if plural_cat in prev.variants:
                error(f"Duplicate {plural_cat}")
            prev.variants[plural_cat] = var_data
        else:
            plurals[msg_id] = XcodePlural(
                var_name, None, variants={plural_cat: var_data}
            )


def parse_pattern(src: str | None) -> Iterator[str | Expression]:
    if not src:
        return
    pos = 0
    for m in printf.finditer(src):
        start = m.start()
        if start > pos:
            yield src[pos:start]
        source = m[0]
        format = source[-1]
        if format == "%":
            yield Expression("%", attributes={"source": source})
        else:
            name: str
            func: str | None
            # TODO post-py38: should be a match
            if format in {"c", "C", "s", "S"}:
                name = "str"
                func = "string"
            elif format in {"d", "D", "o", "O", "p", "u", "U", "x", "X"}:
                name = "int"
                func = "integer"
            elif format in {"a", "A", "e", "E", "f", "g", "G"}:
                name = "num"
                func = "number"
            else:
                name = "arg"
                func = None
            if m[1]:
                name += m[1][0]
            yield Expression(
                VariableRef(name),
                FunctionAnnotation(func) if func else None,
                attributes={"source": source},
            )
        pos = m.end()
    if pos < len(src):
        yield src[pos:]
