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
from re import compile
from typing import Dict, cast

from lxml import etree

from ...message import (
    CatchallKey,
    Expression,
    FunctionAnnotation,
    Message,
    Pattern,
    PatternMessage,
    SelectMessage,
    VariableRef,
)
from ..data import Entry, Metadata, Resource
from .parse import plural_categories, resource_ref, xliff_g, xliff_ns, xml_name


def android_serialize(
    resource: Resource[str, str] | Resource[Message, str],
    trim_comments: bool = False,
) -> Iterator[str]:
    """
    Serialize a resource as an Android strings XML file.

    Section comments and metadata are not supported.
    Resource and entry metadata must be stringifiable,
    as they're stored in XML attributes.

    Messages in '!ENTITY' sections are included in a !DOCTYPE declaration.
    Otherwise, sections must be anonymous.

    Multi-part message identifiers are only supported for <string-array>
    values, for which the second part must be convertible to an int.

    Expressions with a "translate": "no" attribute
    will be wrapped with an <xliff:g> element.
    If such an expression includes a "source" attribute,
    that will be used as the element body
    instead of the literal string or variable name;
    any variable name will be assigned to the element's "id" attribute.

    Markup with a "translate": "no" attribute on both the open and close elements
    will be rendered as <xliff:g> elements.

    Except for "entity" and "reference", function annotations are ignored.
    """

    yield '<?xml version="1.0" encoding="utf-8"?>\n'
    if resource.comment and not trim_comments:
        yield f"\n<!--{comment_body(resource.comment, 0)}-->\n\n"

    # The nsmap needs to be set during creation
    # https://bugs.launchpad.net/lxml/+bug/555602
    root_nsmap: dict[str | None, str] = {}
    root_attrib = {}
    for m in resource.meta:
        k = m.key
        v = str(m.value)
        if k == "xmlns":
            root_nsmap[None] = v
        elif k.startswith("xmlns:"):
            root_nsmap[k[6:]] = v
        else:
            root_attrib[k] = v
    root = etree.Element("resources", attrib=root_attrib, nsmap=root_nsmap)

    entities = []
    string_array = None
    for section in resource.sections:
        if section.meta:
            raise ValueError("Section metadata is not supported")
        if section.comment and not trim_comments:
            add_comment(root, section.comment, True)
        if section.id:
            if section.id == ("!ENTITY",):
                for entry in section.entries:
                    if isinstance(entry, Entry):
                        entities.append(entity_definition(entry))
                continue
            else:
                raise ValueError(f"Unsupported section id: {section.id}")

        for entry in section.entries:
            if isinstance(entry, Entry):
                if len(entry.id) not in (1, 2):
                    raise ValueError(f"Unsupported entry id: {entry.id or entry}")
                name = entry.id[0]
                if not xml_name.fullmatch(name):
                    raise ValueError(f"Invalid entry name: {name}")
                if len(entry.id) == 1:
                    attrib = get_attrib(name, entry.meta)
                    if isinstance(entry.value, SelectMessage):
                        # <plurals>
                        el = etree.SubElement(root, "plurals", attrib=attrib)
                        if entry.comment and not trim_comments:
                            add_comment(el, entry.comment, False)
                        set_plural_message(el, entry.value)
                    else:
                        # <string>
                        if entry.comment and not trim_comments:
                            add_comment(root, entry.comment, False)
                        el = etree.SubElement(root, "string", attrib=attrib)
                        set_pattern_message(el, entry.value)
                else:
                    # <string-array>
                    if string_array is None or name != string_array.get("name"):
                        string_array = etree.SubElement(
                            root, "string-array", attrib=get_attrib(name, entry.meta)
                        )
                    if entry.comment and not trim_comments:
                        add_comment(string_array, entry.comment, False)
                    set_string_array_item(string_array, entry)
            elif not trim_comments:
                add_comment(string_array or root, entry.comment, True)

    etree.cleanup_namespaces(root, {"xliff": xliff_ns})

    # Can't use the built-in pretty-printing,
    # as standalone comments need a trailing empty line.
    if len(root) == 0:
        root.text = "\n"
    else:
        root.text = "\n  "
        for el in root:
            if not el.tail:
                el.tail = "\n  "
            if el.tag in ("plurals", "string-array"):
                if len(el) == 0:
                    el.text = "\n  "
                else:
                    el.text = "\n    "
                    for item in el:
                        item.tail = "\n    "
                    el[-1].tail = "\n  "
        root[-1].tail = "\n"

    if entities:
        yield "<!DOCTYPE resources [\n"
        for entity in entities:
            yield f"  {entity}\n"
        yield "]>\n"
    yield etree.tostring(root, encoding="unicode", method="html")
    yield "\n"


def get_attrib(name: str, meta: list[Metadata[str]]) -> dict[str, str]:
    res = {"name": name}
    for m in meta:
        if m.key == "name":
            raise ValueError(f'Unsupported "name" metadata for {name}')
        res[m.key] = m.value
    return res


def comment_body(content: str, indent: int) -> str:
    # Comments can't include --, so add a zero width space between and after dashes beyond the first
    cc = content.strip().replace("--", "-\u200b-\u200b")
    if "\n" in cc:
        sp = " " * (indent + 2)
        ci = "\n".join(sp + line if line else "" for line in cc.split("\n"))
        return f"\n{ci}\n{' ' * indent}"
    else:
        return f" {cc} "


def add_comment(el: etree._Element, content: str, standalone: bool) -> None:
    indent = 2 if el.tag == "resources" else 4
    comment = etree.Comment(comment_body(content, indent))
    comment.tail = ("\n\n" if standalone else "\n") + (" " * indent)
    el.append(comment)


def entity_definition(entry: Entry[str, str] | Entry[Message, str]) -> str:
    if len(entry.id) != 1 or not xml_name.fullmatch(entry.id[0]):
        raise ValueError(f"Invalid entity identifier: {entry.id}")
    name = entry.id[0]
    if not xml_name.fullmatch(name):
        raise ValueError(f"Invalid entity name: {name}")

    # Characters not allowed in XML EntityValue text
    escape = str.maketrans({"&": "&amp;", "%": "&#37;", '"': "&quot;"})

    if isinstance(entry.value, str):
        value = entry.value.translate(escape)
    elif isinstance(entry.value, PatternMessage) and not entry.value.declarations:
        value = ""
        for part in entry.value.pattern:
            if isinstance(part, str):
                value += part.translate(escape)
            else:
                ref = entity_name(part) if isinstance(part, Expression) else None
                if ref and xml_name.fullmatch(ref):
                    value += f"&{ref};"
                else:
                    raise ValueError(f"Unsupported entity part: {part}")
    else:
        raise ValueError(f"Unsupported entity value: {entry.value}")

    return f'<!ENTITY {name} "{value}">'


def set_string_array_item(
    parent: etree._Element, entry: Entry[str, str] | Entry[Message, str]
) -> None:
    try:
        num = int(entry.id[1])
    except ValueError:
        raise ValueError(f"Unsupported entry id: {entry.id}")
    if num != len(parent):
        raise ValueError(f"String-array keys must be ordered: {entry.id}")
    if isinstance(entry.value, SelectMessage):
        raise ValueError(f"Unsupported message type for {entry.id}: {entry.value}")
    item = etree.SubElement(parent, "item")
    set_pattern_message(item, entry.value)


def set_plural_message(plurals: etree._Element, msg: SelectMessage) -> None:
    sel = msg.selectors[0] if len(msg.selectors) == 1 else None
    if (
        msg.declarations
        or not sel
        or not isinstance(sel.annotation, FunctionAnnotation)
        or sel.annotation.name != "number"
    ):
        raise ValueError(f"Unsupported message: {msg}")
    for keys, value in msg.variants.items():
        key = keys[0] if len(keys) == 1 else None
        if isinstance(key, CatchallKey):
            key = key.value or "other"
        if key not in plural_categories:
            raise ValueError(f"Unsupported plural variant key: {keys}")
        item = etree.SubElement(plurals, "item", attrib={"quantity": key})
        set_pattern(item, value)
        item.tail = "\n    "
    item.tail = "\n  "


def set_pattern_message(el: etree._Element, msg: PatternMessage | str) -> None:
    if isinstance(msg, str):
        el.text = escape_backslash(msg)
        escape_doublequote(el)
    elif isinstance(msg, PatternMessage) and not msg.declarations:
        set_pattern(el, msg.pattern)
    else:
        raise ValueError(f"Unsupported message: {msg}")


def set_pattern(el: etree._Element, pattern: Pattern) -> None:
    node: etree._Element | None
    if len(pattern) == 1 and isinstance(pattern[0], Expression):
        annot = pattern[0].annotation
        if isinstance(annot, FunctionAnnotation) and annot.name == "reference":
            # A "string" could be an Android resource reference,
            # which should not have its @ or ? sigil escaped.
            arg = pattern[0].arg
            if isinstance(arg, str) and resource_ref.fullmatch(arg):
                el.text = arg
                return
            else:
                raise ValueError(f"Invalid reference value: {arg}")

    parent = el
    node = None
    for part in pattern:
        if isinstance(part, str):
            esc = escape_backslash(part)
            if node is None:
                parent.text = parent.text + esc if parent.text else esc
            else:
                node.tail = node.tail + esc if node.tail else esc
        elif isinstance(part, Expression):
            ent_name = entity_name(part)
            if part.attributes.get("translate", None) == "no":
                # <xliff:g>
                attrib = (
                    cast(Dict[str, str], part.annotation.options)
                    if isinstance(part.annotation, FunctionAnnotation)
                    else None
                )
                nsmap = {"xliff": xliff_ns} if not el.nsmap.get("xliff", None) else None
                node = etree.SubElement(parent, xliff_g, attrib=attrib, nsmap=nsmap)
                if ent_name:
                    node.append(etree.Entity(ent_name))
                elif "source" in part.attributes:
                    source = part.attributes["source"]
                    if source:
                        node.text = str(source)
                else:
                    if isinstance(part.arg, str):
                        node.text = escape_backslash(part.arg)
                    elif isinstance(part.arg, VariableRef):
                        node.text = part.arg.name
            elif ent_name:
                node = etree.Entity(ent_name)
                parent.append(node)
            elif "source" in part.attributes:
                source = part.attributes["source"]
                if not isinstance(source, str):
                    raise ValueError(f"Unsupported expression source: {part}")
                if node is None:
                    parent.text = parent.text + source if parent.text else source
                else:
                    node.tail = node.tail + source if node.tail else source
            else:
                source = None
                if isinstance(part.arg, str):
                    source = escape_backslash(part.arg)
                elif isinstance(part.arg, VariableRef):
                    source = part.arg.name
                if source is not None:
                    if node is None:
                        parent.text = parent.text + source if parent.text else source
                    else:
                        node.tail = node.tail + source if node.tail else source
                else:
                    raise ValueError(f"Unsupported expression: {part}")
        elif any(isinstance(value, VariableRef) for value in part.options.values()):
            raise ValueError(f"Unsupported markup with variable option: {part}")
        else:
            if part.attributes.get("translate", None) == "no":
                name = f"{{{xliff_ns}}}g"
            elif ":" in part.name:
                ns, local = part.name.split(":", 1)
                xmlns = el.nsmap.get(ns, xliff_ns if ns == "xliff" else ns)
                name = f"{{{xmlns}}}{local}"
            else:
                name = part.name
            attrib = cast(Dict[str, str], part.options)
            if part.kind == "standalone":
                node = etree.SubElement(parent, name, attrib=attrib)
            elif part.kind == "open":
                parent = etree.SubElement(parent, name, attrib=attrib)
                node = None
            elif parent != el and name == parent.tag:  # kind == 'close'
                node = parent
                parent = cast(etree._Element, parent.getparent())
            else:
                raise ValueError(f"Improper element nesting for {part} in {parent}")
    escape_doublequote(el)


def entity_name(part: Expression) -> str | None:
    if (
        isinstance(part.annotation, FunctionAnnotation)
        and part.annotation.name == "entity"
    ):
        name = part.arg.name if isinstance(part.arg, VariableRef) else None
        if name:
            return name
        else:
            raise ValueError(f"Invalid entity exression: {part}")
    return None


# Special Android characters
android_escape = str.maketrans(
    {"\\": r"\\", "\n": r"\n", "\t": r"\t", "'": r"\'", '"': r"\""}
)

# Control codes are not valid in XML, and nonstandard whitespace is hard to see.
control_chars = compile(r"[\x00-\x19\x7F-\x9F]|[^\S ]")


def escape_backslash(src: str) -> str:
    res = src.translate(android_escape)
    return control_chars.sub(lambda m: f"\\u{ord(m.group()):04d}", res)


def escape_doublequote(el: etree._Element, root: bool = True) -> None:
    if el.text and ("  " in el.text):
        el.text = f'"{el.text}"'
    for child in el:
        escape_doublequote(child, False)
        if child.tail and "  " in child.tail:
            child.tail = f'"{child.tail}"'
    if root:
        if el.text and el.text.startswith((" ", "@", "?")):
            el.text = f'"{el.text}"'
        if len(el) > 0:
            last = el[-1]
            if last.tail and last.tail.endswith(" "):
                last.tail = f'"{last.tail}"'
        elif el.text and el.text.endswith(" "):
            el.text = f'"{el.text}"'
