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

from collections.abc import Callable
from re import fullmatch
from typing import Any, Iterator

from fluent.syntax import FluentSerializer
from fluent.syntax import ast as ftl

from ... import message as msg
from .. import data as res


def fluent_serialize(
    resource: (
        res.Resource[str, res.M]
        | res.Resource[msg.Message, res.M]
        | res.Resource[ftl.Pattern, res.M]
    ),
    serialize_metadata: Callable[[res.Metadata[res.M]], str | None] | None = None,
    trim_comments: bool = False,
) -> Iterator[str]:
    """
    Serialize a resource as the contents of a Fluent FTL file.

    Section identifiers are not supported.
    Single-part message identifiers are treated as message values,
    while two-part message identifiers are considered message attributes.

    Function names are upper-cased, and annotations with the `message` function
    are mapped to message and term references.

    Yields each entry and comment separately.
    If the resource includes any metadata, a `serialize_metadata` callable must be provided
    to map each field into a comment value, or to discard it by returning an empty value.
    """
    ftl_ast = fluent_astify(resource, serialize_metadata, trim_comments)
    serializer = FluentSerializer()
    nl_prefix = 0
    for entry in ftl_ast.body:
        yield serializer.serialize_entry(entry, nl_prefix)
        if not nl_prefix:
            nl_prefix = 1


def fluent_astify(
    resource: (
        res.Resource[str, res.M]
        | res.Resource[msg.Message, res.M]
        | res.Resource[ftl.Pattern, res.M]
    ),
    serialize_metadata: Callable[[res.Metadata[res.M]], str | None] | None = None,
    trim_comments: bool = False,
) -> ftl.Resource:
    """
    Transform a resource into a corresponding Fluent AST structure.

    Section identifiers are not supported.
    Single-part message identifiers are treated as message values,
    while two-part message identifiers are considered message attributes.

    Function names are upper-cased, and annotations with the `message` function
    are mapped to message and term references.

    If the resource includes any metadata other than a string resource `info` value,
    a `serialize_metadata` callable must be provided
    to map each field into a comment value, or to discard it by returning an empty value.
    """

    def comment(
        node: (
            res.Resource[Any, Any]
            | res.Section[Any, Any]
            | res.Entry[Any, Any]
            | res.Comment
        ),
    ) -> str:
        if trim_comments:
            return ""
        cs = node.comment.rstrip()
        if not isinstance(node, res.Comment) and node.meta:
            if not serialize_metadata:
                raise ValueError("Metadata requires serialize_metadata parameter")
            for field in node.meta:
                if (
                    isinstance(node, res.Resource)
                    and field.key == "info"
                    and field == node.meta[0]
                ):
                    continue
                meta_str = serialize_metadata(field)
                if meta_str:
                    ms = meta_str.strip("\n")
                    cs = f"{cs}\n{ms}" if cs else ms
        return cs

    body: list[ftl.EntryType] = []
    res_info = resource.meta[0] if resource.meta else None
    if (
        not trim_comments
        and res_info
        and res_info.key == "info"
        and isinstance(res_info.value, str)
        and res_info.value
    ):
        body.append(ftl.Comment(res_info.value))
        res_comment = resource.comment.rstrip()
    else:
        res_comment = comment(resource)
    if res_comment:
        body.append(ftl.ResourceComment(res_comment))
    for idx, section in enumerate(resource.sections):
        section_comment = comment(section)  # type: ignore[arg-type]
        if not trim_comments and idx != 0 or section_comment:
            body.append(ftl.GroupComment(section_comment))
        cur: ftl.Message | ftl.Term | None = None
        cur_id = ""
        for entry in section.entries:  # type: ignore[attr-defined]
            if isinstance(entry, res.Comment):
                if not trim_comments:
                    body.append(ftl.Comment(entry.comment))
                cur = None
            else:
                value = (
                    entry.value
                    if isinstance(entry.value, ftl.Pattern)
                    else fluent_astify_message(entry.value)
                )
                entry_comment = comment(entry)
                if len(entry.id) == 1:  # value
                    cur_id = entry.id[0]
                    cur = (
                        ftl.Term(ftl.Identifier(cur_id[1:]), value)
                        if cur_id.startswith("-")
                        else ftl.Message(ftl.Identifier(cur_id), value)
                    )
                    if entry_comment:
                        cur.comment = ftl.Comment(entry_comment)
                    body.append(cur)
                elif len(entry.id) == 2:  # attribute
                    if cur is None or entry.id[0] != cur_id:
                        cur_id = entry.id[0]
                        if cur_id.startswith("-"):
                            value = ftl.Pattern([ftl.Placeable(ftl.StringLiteral(""))])
                            cur = ftl.Term(ftl.Identifier(cur_id[1:]), value)
                        else:
                            cur = ftl.Message(ftl.Identifier(cur_id))
                        if entry_comment:
                            cur.comment = ftl.Comment(entry_comment)
                        body.append(cur)
                    elif entry_comment:
                        attr_comment = f"{entry.id[1]}:\n{entry_comment}"
                        if cur.comment:
                            cur.comment.content = (
                                str(cur.comment.content) + "\n\n" + attr_comment
                            )
                        else:
                            cur.comment = ftl.Comment(attr_comment)
                    cur.attributes.append(
                        ftl.Attribute(ftl.Identifier(entry.id[1]), value)
                    )
                else:
                    raise ValueError(f"Unsupported message id: {entry.id}")
    return ftl.Resource(body)


def fluent_astify_message(message: str | msg.Message) -> ftl.Pattern:
    """
    Transform a message into a corresponding Fluent AST pattern.

    Function names are upper-cased, and annotations with the `message` function
    are mapped to message and term references.
    """

    if isinstance(message, str):
        return ftl.Pattern([ftl.TextElement(message)])
    if not isinstance(message, (msg.PatternMessage, msg.SelectMessage)):
        raise ValueError(f"Unsupported message: {message}")
    decl = [d for d in message.declarations if isinstance(d, msg.Declaration)]
    if len(decl) != len(message.declarations):
        raise ValueError("Unsupported statements are not supported")
    if isinstance(message, msg.PatternMessage):
        return flat_pattern(decl, message.pattern)

    # It gets a bit complicated for SelectMessage. We'll be modifying this list,
    # building select expressions for each selector starting from the last one
    # until this list has only one entry `[[], pattern]`.
    #
    # We rely on the variants being in order, so that a variant with N keys
    # will be next to all other variants for which the first N-1 keys are equal.
    variants = [
        (list(keys), flat_pattern(decl, value))
        for keys, value in message.variants.items()
    ]

    other = fallback_name(message.variants)
    keys0 = variants[0][0]
    while keys0:
        selector = expression(decl, message.selectors[len(keys0) - 1])
        if (
            isinstance(selector, ftl.FunctionReference)
            and selector.id.name == "NUMBER"
            and selector.arguments.positional
            and isinstance(selector.arguments.positional[0], ftl.VariableReference)
            and not selector.arguments.named
        ):
            selector = selector.arguments.positional[0]
        base_keys = []
        sel_exp = None
        i = 0
        while i < len(variants):
            keys, pattern = variants[i]
            key = keys.pop()  # Ultimately modifies keys0
            ftl_variant = ftl.Variant(
                variant_key(key, other), pattern, isinstance(key, msg.CatchallKey)
            )
            if sel_exp and keys == base_keys:
                sel_exp.variants.append(ftl_variant)
                variants.pop(i)
            else:
                base_keys = keys
                sel_exp = ftl.SelectExpression(selector.clone(), [ftl_variant])
                variants[i] = (keys, ftl.Pattern([ftl.Placeable(sel_exp)]))
                i += 1
    if len(variants) != 1:
        raise ValueError(f"Error resolving select message variants (n={len(variants)})")
    return variants[0][1]


def fallback_name(variants: msg.Variants) -> str:
    """
    Try `other`, `other1`, `other2`, ... until a free one is found.
    """
    i = 0
    key = root = "other"
    while any(
        key == (k.value if isinstance(k, msg.CatchallKey) else k)
        for keys in variants
        for k in keys
    ):
        i += 1
        key = f"{root}{i}"
    return key


def variant_key(
    key: str | msg.CatchallKey, other: str
) -> ftl.NumberLiteral | ftl.Identifier:
    kv = key.value or other if isinstance(key, msg.CatchallKey) else key
    try:
        float(kv)
        return ftl.NumberLiteral(kv)
    except Exception:
        if fullmatch(r"[a-zA-Z][\w-]*", kv):
            return ftl.Identifier(kv)
        raise ValueError(f"Unsupported variant key: {kv}")


def flat_pattern(decl: list[msg.Declaration], pattern: msg.Pattern) -> ftl.Pattern:
    elements: list[ftl.TextElement | ftl.Placeable] = []
    for el in pattern:
        if isinstance(el, str):
            elements.append(ftl.TextElement(el))
        elif isinstance(el, msg.Expression):
            elements.append(ftl.Placeable(expression(decl, el)))
        else:
            raise ValueError(f"Conversion to Fluent not supported: {el}")
    return ftl.Pattern(elements)


def expression(
    decl: list[msg.Declaration], expr: msg.Expression
) -> ftl.InlineExpression:
    arg = value(decl, expr.arg) if expr.arg is not None else None
    if isinstance(expr.annotation, msg.FunctionAnnotation):
        return function_ref(decl, arg, expr.annotation)
    elif expr.annotation:
        raise ValueError("Unsupported annotations are not supported")
    if arg:
        return arg
    raise ValueError("Invalid empty expression")


def function_ref(
    decl: list[msg.Declaration],
    arg: ftl.InlineExpression | None,
    annotation: msg.FunctionAnnotation,
) -> ftl.InlineExpression:
    named: list[ftl.NamedArgument] = []
    for name, val in annotation.options.items():
        ftl_val = value(decl, val)
        if isinstance(ftl_val, ftl.Literal):
            named.append(ftl.NamedArgument(ftl.Identifier(name), ftl_val))
        else:
            raise ValueError(f"Fluent option value not literal for {name}: {ftl_val}")

    if annotation.name == "string":
        if not arg:
            raise ValueError("Argument required for :string")
        if named:
            raise ValueError("Options on :string are not supported")
        return arg
    if annotation.name == "number" and isinstance(arg, ftl.NumberLiteral) and not named:
        return arg
    if annotation.name == "message":
        if not isinstance(arg, ftl.Literal):
            raise ValueError(
                "Message and term references must have a literal message identifier"
            )
        match = fullmatch(r"(-?[a-zA-Z][\w-]*)(?:\.([a-zA-Z][\w-]*))?", arg.value)
        if not match:
            raise ValueError(f"Invalid message or term identifier: {arg.value}")
        msg_id = match[1]
        msg_attr = match[2]
        attr = ftl.Identifier(msg_attr) if msg_attr else None
        if msg_id[0] == "-":
            args = ftl.CallArguments(named=named) if named else None
            return ftl.TermReference(ftl.Identifier(msg_id[1:]), attr, args)
        elif named:
            raise ValueError("Options on message references are not supported")
        else:
            return ftl.MessageReference(ftl.Identifier(msg_id), attr)

    args = ftl.CallArguments([arg] if arg else None, named)
    return ftl.FunctionReference(ftl.Identifier(annotation.name.upper()), args)


# Non-printable ASCII C0 & C1 / Unicode Cc characters
esc_cc = {n: f"\\u{n:04X}" for r in (range(0, 32), range(127, 160)) for n in r}


def value(
    decl: list[msg.Declaration], val: str | msg.VariableRef
) -> ftl.InlineExpression:
    if isinstance(val, str):
        try:
            float(val)
            return ftl.NumberLiteral(val)
        except Exception:
            return ftl.StringLiteral(val.translate(esc_cc))
    else:
        local = next((d for d in decl if d.name == val.name), None)
        return (
            expression(decl, local.value)
            if local
            else ftl.VariableReference(ftl.Identifier(val.name))
        )
