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

from typing import Callable

from ..message import Message
from .data import Resource
from .dtd.parse import dtd_parse
from .fluent.parse import fluent_parse
from .format import Format, detect_format
from .inc.parse import inc_parse
from .ini.parse import ini_parse
from .plain_json.parse import plain_json_parse
from .po.parse import po_parse
from .properties.parse import properties_parse
from .webext.parse import webext_parse

android_parse: Callable[[str | bytes], Resource[Message, str]] | None
xliff_parse: Callable[[str | bytes], Resource[Message, str]] | None
try:
    from .android.parse import android_parse
    from .xliff.parse import xliff_parse
except ImportError:
    android_parse = None
    xliff_parse = None


class UnsupportedResource(Exception):
    pass


def parse_resource(
    input: Format | str | None, source: str | bytes | None = None
) -> Resource[Message, str]:
    """
    Parse a Resource from its string representation.

    The first argument may be an explicit Format,
    the file path as a string, or None.
    For the latter two types,
    an attempt is made to detect the appropriate format.

    If the first argument is a string path,
    the `source` argument is optional,
    as the file will be opened and read.
    """
    if source is None:
        if not isinstance(input, str):
            raise TypeError("Source is required if type is not a string path")
        with open(input, mode="rb") as file:
            source = file.read()
    # TODO post-py38: should be a match
    format = input if isinstance(input, Format) else detect_format(input, source)
    if format == Format.dtd:
        return dtd_parse(source)
    elif format == Format.fluent:
        return fluent_parse(source)
    elif format == Format.inc:
        return inc_parse(source)
    elif format == Format.ini:
        return ini_parse(source)
    elif format == Format.plain_json:
        return plain_json_parse(source)
    elif format == Format.po:
        return po_parse(source)
    elif format == Format.properties:
        return properties_parse(source)
    elif format == Format.webext:
        return webext_parse(source)
    elif format == Format.android and android_parse is not None:
        return android_parse(source)
    elif format == Format.xliff and xliff_parse is not None:
        return xliff_parse(source)
    else:
        raise UnsupportedResource(f"Unsupported resource format: {input}")
