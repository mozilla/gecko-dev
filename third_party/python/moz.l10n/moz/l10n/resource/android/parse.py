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

from collections.abc import Callable, Iterable, Iterator
from re import compile

from lxml import etree

from ...message import (
    CatchallKey,
    Expression,
    FunctionAnnotation,
    Markup,
    Message,
    PatternMessage,
    SelectMessage,
    VariableRef,
)
from ..data import Comment, Entry, Metadata, Resource, Section
from ..format import Format

plural_categories = ("zero", "one", "two", "few", "many", "other")
xliff_ns = "urn:oasis:names:tc:xliff:document:1.2"
xliff_g = f"{{{xliff_ns}}}g"

# Exclude : for compatibility with MF2
xml_name_start = r"A-Z_a-z\xC0-\xD6\xD8-\xF6\xF8-\u02FF\u0370-\u037D\u037F-\u1FFF\u200C-\u200D\u2070-\u218F\u2C00-\u2FEF\u3001-\uD7FF\uF900-\uFDCF\uFDF0-\uFFFD\U00010000-\U000EFFFF"
xml_name_rest = r".0-9\xB7\u0300-\u036F\u203F-\u2040-"
xml_name = compile(f"[{xml_name_start}][{xml_name_start}{xml_name_rest}]*")

# Android string resources contain four different kinds of localizable values:
#
#   - HTML entity declarations,
#     which will be inserted into other strings during XML parsing.
#   - Strings with printf-style variables,
#     which also use "quotes" for special escaping behaviour.
#     These may include HTML as escaped string contents,
#     which will require fromHtml(String) processing
#     after being initially formatted with getString(int, Object...)
#   - Strings with HTML contents, which can't include variables,
#     and are generally used via setText(java.lang.CharSequence).
#   - Strings with ICU MessageFormat contents.
#     These also use "quotes" for special escaping behaviour.
#     ICU MessageFormat strings are not currently detected by this library.
#
# The source contents of each of the above needs to be parsed differently,
# and message strings can be found in <string>, <string-array>, and <plurals>
# elements, each of which also needs different parsing.
#
# For more information, see:
# https://developer.android.com/guide/topics/resources/string-resource
# https://developer.android.com/guide/topics/resources/localization#mark-message-parts


def android_parse(source: str | bytes) -> Resource[Message, str]:
    """
    Parse an Android strings XML file into a message resource.

    If any internal DOCTYPE entities are declared,
    they are included as messages in an "!ENTITY" section.

    Resource and entry attributes are parsed as metadata.

    All XML, Android, and printf escapes are unescaped
    except for %n, which has a platform-dependent meaning.

    Spans of text and entities wrapped in an <xliff:g>
    will be parsed as expressions with a "translate": "no" attribute.
    Spans including elements will be wrapped with open/close markup
    with a similar attribute.
    """
    parser = etree.XMLParser(resolve_entities=False)
    root = etree.fromstring(
        source.encode() if isinstance(source, str) else source, parser
    )
    if root.tag != "resources":
        raise ValueError(f"Unsupported root node: {root}")
    if root.text and not root.text.isspace():
        raise ValueError(f"Unexpected text in resource: {root.text}")
    res: Resource[Message, str] = Resource(Format.android, [Section((), [])])
    root_comments = [c.text for c in root.itersiblings(etree.Comment, preceding=True)]
    if root_comments:
        root_comments.reverse()
        res.comment = comment_str(root_comments)
    res.meta = [Metadata(k, v) for k, v in root.attrib.items()]
    for ns, url in root.nsmap.items():
        res.meta.append(Metadata(f"xmlns:{ns}" if ns else "xmlns", url))
    entries = res.sections[0].entries

    dtd = root.getroottree().docinfo.internalDTD
    if dtd:
        entities: list[Entry[Message, str] | Comment] = []
        for entity in dtd.iterentities():
            name = entity.name
            if not name:
                raise ValueError(f"Unnamed entity: {entity}")
            value: Message = PatternMessage(list(parse_entity_value(entity.content)))
            entities.append(Entry((name,), value))
        if entities:
            res.sections.insert(0, Section(("!ENTITY",), entities))

    comment: list[str | None] = []  # TODO: should be list[str]
    for el in root:
        if el.tail and not el.tail.isspace():
            raise ValueError(f"Unexpected text in resource: {el.tail}")
        if isinstance(el, etree._Comment):
            comment.append(el.text)
            if el.tail and el.tail.count("\n") > 1 and comment:
                entries.append(Comment(comment_str(comment)))
                comment.clear()
        else:
            name = el.attrib.get("name", None)
            if not name:
                raise ValueError(f"Unnamed {el.tag} entry: {el}")
            meta = [Metadata(k, v) for k, v in el.attrib.items() if k != "name"]

            if el.tag == "string":
                value = PatternMessage(list(parse_pattern(el)))
                entries.append(Entry((name,), value, comment_str(comment), meta))

            elif el.tag == "plurals":
                if el.text and not el.text.isspace():
                    raise ValueError(f"Unexpected text in {name} plurals: {el.text}")
                value = parse_plurals(name, el, comment.extend)
                entries.append(Entry((name,), value, comment_str(comment), meta))

            elif el.tag == "string-array":
                if el.text and not el.text.isspace():
                    raise ValueError(
                        f"Unexpected text in {name} string-array: {el.text}"
                    )
                idx = 0
                for item in el:
                    if isinstance(item, etree._Comment):
                        comment.append(item.text)
                    elif item.tag == "item":
                        value = PatternMessage(list(parse_pattern(item)))
                        ic = comment_str(comment)
                        entries.append(Entry((name, str(idx)), value, ic, meta[:]))
                        comment.clear()
                        idx += 1
                    else:
                        cs = etree.tostring(item, encoding="unicode")
                        raise ValueError(f"Unsupported {name} string-array child: {cs}")
                    if item.tail and not item.tail.isspace():
                        raise ValueError(
                            f"Unexpected text in {name} string-array: {item.tail}"
                        )

            else:
                es = etree.tostring(el, encoding="unicode")
                raise ValueError(f"Unsupported entry: {es}")
            if comment:
                comment.clear()
    return res


dash_indent = compile(r" .+(\n   - .*)+ ")


def comment_str(body: list[str | None]) -> str:
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


entity_ref = compile(f"&({xml_name.pattern});")


def parse_entity_value(src: str | None) -> Iterator[str | Expression]:
    if src:
        pos = 0
        for m in entity_ref.finditer(src):
            start = m.start()
            if start > pos:
                yield src[pos:start]
            yield Expression(VariableRef(m[1]), FunctionAnnotation("entity"))
            pos = m.end()
        if pos < len(src):
            yield src[pos:]


def parse_plurals(
    name: str, el: etree._Element, add_comment: Callable[[Iterable[str | None]], None]
) -> SelectMessage:
    sel = Expression(VariableRef("quantity"), FunctionAnnotation("number"))
    msg = SelectMessage([sel], {})
    var_comment: list[str | None] = []
    for item in el:
        if isinstance(item, etree._Comment):
            var_comment.append(item.text)
        elif item.tag == "item":
            key = item.attrib.get("quantity", None)
            if key not in plural_categories:
                raise ValueError(f"Invalid quantity for {name} plurals item: {key}")
            if var_comment:
                add_comment(
                    (f"{key}: {c}" for c in var_comment if c)
                    if msg.variants
                    else var_comment
                )
                var_comment.clear()
            msg.variants[(CatchallKey(key) if key == "other" else key,)] = list(
                parse_pattern(item)
            )
        else:
            cs = etree.tostring(item, encoding="unicode")
            raise ValueError(f"Unsupported {name} plurals child: {cs}")
        if item.tail and not item.tail.isspace():
            raise ValueError(f"Unexpected text in {name} plurals: {item.tail}")
    return msg


resource_ref = compile(r"@(?:\w+:)?\w+/\w+|\?(?:\w+:)?(\w+/)?\w+")


def parse_pattern(el: etree._Element) -> Iterator[str | Expression | Markup]:
    if len(el) == 0 and el.text and resource_ref.fullmatch(el.text):
        # https://developer.android.com/guide/topics/resources/providing-resources#ResourcesFromXml
        yield Expression(el.text, FunctionAnnotation("reference"))
    else:
        flat = flatten(el)
        spaced = parse_quotes(flat)
        yield from parse_inline(spaced)


def flatten(el: etree._Element) -> Iterator[str | Expression | Markup]:
    if el.text:
        yield el.text
    for child in el:
        if isinstance(child, etree._Entity):
            yield Expression(VariableRef(child.name), FunctionAnnotation("entity"))
        else:
            name = (
                f"{child.prefix}:{etree.QName(child.tag).localname}"
                if child.prefix
                else child.tag
            )
            if child.tag == xliff_g:
                body = list(flatten(child))
                if any(
                    isinstance(gc, Expression)
                    and gc.attributes.get("translate", None) == "no"
                    or isinstance(gc, Markup)
                    for gc in body
                ):
                    # Any <xliff:g> around elements needs to be rendered explicitly
                    yield Markup("open", name, dict(child.attrib), {"translate": "no"})
                    yield from body
                    yield Markup("close", name, attributes={"translate": "no"})
                else:
                    id = child.get("id", None)
                    for gc in body:
                        if isinstance(gc, str):
                            options: dict[str, str | VariableRef] = dict(child.attrib)
                            attr: dict[str, str | VariableRef | None] = {
                                "translate": "no"
                            }
                            arg: str | VariableRef | None
                            if id:
                                arg = VariableRef(get_var_name(id))
                                attr["source"] = gc
                            elif gc.startswith(("%", "{")):
                                arg = VariableRef(get_var_name(gc))
                                attr["source"] = gc
                            else:
                                arg = gc
                            yield Expression(
                                arg,
                                FunctionAnnotation(name, options) if options else None,
                                attr,
                            )
                        else:
                            gc.attributes["translate"] = "no"
                            gc.annotation.options = dict(child.attrib)  # type: ignore[union-attr]
                            yield gc
            else:
                yield Markup("open", name, options=dict(child.attrib))
                yield from flatten(child)
                yield Markup("close", name)
        if child.tail:
            yield child.tail


double_quote = compile(r'(?<!\\)"')
spaces = compile(r"\s+")


def parse_quotes(
    iter: Iterator[str | Expression | Markup],
) -> Iterator[str | Expression | Markup]:
    stack: list[str | Expression] = []

    def collapse_stack() -> Iterator[str | Expression | Markup]:
        yield '"'
        for part in stack:
            yield spaces.sub(" ", part) if isinstance(part, str) else part

    for part in iter:
        if isinstance(part, str):
            pos = 0
            quoted = bool(stack)
            for m in double_quote.finditer(part):
                prev = part[pos : m.start()]
                if quoted:
                    if stack:
                        yield from stack
                        stack.clear()
                    if prev:
                        yield prev
                elif prev:
                    yield spaces.sub(" ", prev)
                quoted = not quoted
                pos = m.end()
            last = part[pos:]
            if quoted:
                stack.append(last)
            elif last:
                yield spaces.sub(" ", last)
        elif stack:
            if (
                isinstance(part, Markup)
                or part.attributes.get("translate", None) == "no"
            ):
                yield from collapse_stack()
                stack.clear()
                yield part
            else:  # Expression
                stack.append(part)
        else:
            yield part
    if stack:
        yield from collapse_stack()


inline_re = compile(
    r"""\\([@?nt'"\\])|"""
    r"\\u([0-9]{4})|"
    r"(<[^%>]+>)|"
    r"(%(?:[1-9]\$)?[-#+ 0,(]?[0-9.]*([a-su-zA-SU-Z%]|[tT][a-zA-Z]))"
)


def parse_inline(
    iter: Iterator[str | Expression | Markup],
) -> Iterator[str | Expression | Markup]:
    acc = ""
    for part in iter:
        if not isinstance(part, str):
            if acc:
                yield acc
                acc = ""
            yield part
        else:
            pos = 0
            for m in inline_re.finditer(part):
                start = m.start()
                if start > pos:
                    acc += part[pos:start]
                if m[1]:
                    # Special character
                    c = m[1]
                    acc += "\n" if c == "n" else "\t" if c == "t" else c
                elif m[2]:
                    # Unicode escape
                    acc += chr(int(m[2]))
                elif m[3]:
                    # Escaped HTML element, e.g. &lt;b>
                    # HTML elements containing internal % formatting are not wrapped as literals
                    if acc:
                        yield acc
                        acc = ""
                    yield Expression(m[3], FunctionAnnotation("html"))
                else:
                    if acc:
                        yield acc
                        acc = ""
                    conversion = m[5]
                    if conversion == "%":
                        # Literal %
                        yield Expression("%", attributes={"source": m[4]})
                    else:
                        # Placeholder
                        func: str | None
                        # TODO post-py38: should be a match
                        if conversion in {"b", "B"}:
                            func = "boolean"
                        elif conversion in {"c", "C", "s", "S"}:
                            func = "string"
                        elif conversion in {"d", "h", "H", "o", "x", "X"}:
                            func = "integer"
                        elif conversion in {"a", "A", "e", "E", "f", "g", "G"}:
                            func = "number"
                        else:
                            c0 = conversion[0]
                            func = "datetime" if c0 == "t" or c0 == "T" else None
                        name = get_var_name(m[4])
                        yield Expression(
                            VariableRef(name),
                            FunctionAnnotation(func) if func else None,
                            {"source": m[4]},
                        )
                pos = m.end()
            acc += part[pos:]
    if acc:
        yield acc


printf = compile(r"%([1-9]\$)?")
not_name_char = compile(f"[^{xml_name_start}{xml_name_rest}]")
not_name_start = compile(f"[^{xml_name_start}]")


def get_var_name(src: str) -> str:
    """Returns a valid MF2 name."""
    pm = printf.match(src)
    if pm:
        return f"arg{pm[1][0]}" if pm[1] else "arg"
    name = not_name_char.sub("", src)
    if not_name_start.match(name):
        name = name[1:]
    return name or "arg"
