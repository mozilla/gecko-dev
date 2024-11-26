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

from collections import OrderedDict

from polib import pofile

from ...message import (
    Expression,
    FunctionAnnotation,
    Message,
    PatternMessage,
    SelectMessage,
    VariableRef,
    Variants,
)
from ..data import Entry, Metadata, Resource, Section
from ..format import Format


def po_parse(source: str | bytes) -> Resource[Message, str]:
    """
    Parse a .po or .pot file into a message resource

    Messages may include the following metadata:
    - `translator-comments`
    - `extracted-comments`
    - `reference`: `f"{file}:{line}"`, separately for each reference
    - `obsolete`: `""`
    - `flag`: separately for each flag
    - `plural`
    """
    pf = pofile(source if isinstance(source, str) else source.decode())
    res_comment = pf.header.lstrip("\n").rstrip()
    res_meta: list[Metadata[str]] = [
        Metadata(key, value.strip()) for key, value in pf.metadata.items()
    ]
    sections: dict[str | None, Section[Message, str]] = OrderedDict()
    for pe in pf:
        meta: list[Metadata[str]] = []
        if pe.tcomment:
            meta.append(Metadata("translator-comments", pe.tcomment))
        if pe.comment:
            meta.append(Metadata("extracted-comments", pe.comment))
        for file, line in pe.occurrences:
            meta.append(Metadata("reference", f"{file}:{line}"))
        if pe.obsolete:
            meta.append(Metadata("obsolete", "true"))
        for flag in pe.flags:
            meta.append(Metadata("flag", flag))
        if pe.msgid_plural:
            meta.append(Metadata("plural", pe.msgid_plural))
        if pe.msgstr_plural:
            keys = list(pe.msgstr_plural)
            keys.sort()
            sel = Expression(VariableRef("n"), FunctionAnnotation("number"))
            variants: Variants = {
                (str(idx),): (
                    [pe.msgstr_plural[idx]] if idx in pe.msgstr_plural else []
                )
                for idx in range(keys[-1] + 1)
            }
            value: Message = SelectMessage([sel], variants)
        else:
            value = PatternMessage([pe.msgstr])
        entry = Entry((pe.msgid,), value, meta=meta)
        if pe.msgctxt in sections:
            sections[pe.msgctxt].entries.append(entry)
        else:
            sections[pe.msgctxt] = Section((pe.msgctxt,) if pe.msgctxt else (), [entry])
    return Resource(Format.po, list(sections.values()), res_comment, res_meta)
