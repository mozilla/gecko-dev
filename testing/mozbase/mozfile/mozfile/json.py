# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
This module is a compatibility shim between stdlib json and orjson.
It uses orjson if available, and falls back to stdlib json.

It only supports features and parameters that both modules support,
and attempts to produce identical output regardless of which module
is used.

The interface is a subset of what's supported by stdlib json.
"""

from __future__ import annotations

import json
from typing import TYPE_CHECKING, Any, Callable

if TYPE_CHECKING:
    from _typeshed import SupportsRead, SupportsWrite

try:
    import orjson

    JSONDecodeError = orjson.JSONDecodeError
    JSONEncodeError = orjson.JSONEncodeError
except ImportError:
    orjson = None
    JSONDecodeError = json.JSONDecodeError
    # std-lib raises TypeError on bad input
    JSONEncodeError = TypeError

JSONDecoder = json.JSONDecoder
JSONEncoder = json.JSONEncoder

__all__ = [
    "load",
    "loads",
    "dump",
    "dumps",
    "JSONDecodeError",
    "JSONEncodeError",
    "JSONDecoder",
    "JSONEncoder",
]


def loads(s: str | bytes | bytearray) -> Any:
    if orjson:
        return orjson.loads(s)

    return json.loads(s)


def load(fh: SupportsRead[str | bytes]) -> Any:
    if orjson:
        return loads(fh.read())
    return json.load(fh)


def dumps(
    obj: Any,
    default: Callable[[Any], Any] | None = None,
    indent: int | None = None,
    sort_keys: bool = False,
) -> str:
    if indent and indent != 2:
        raise ValueError("An indent other than 2 is not supported!")

    if orjson:
        option = 0
        if indent:
            option |= orjson.OPT_INDENT_2

        if sort_keys:
            option |= orjson.OPT_SORT_KEYS

        return orjson.dumps(obj, default=default, option=option).decode("utf-8")

    separators = (",", ": ") if indent else (",", ":")
    return json.dumps(
        obj,
        default=default,
        indent=indent,
        sort_keys=sort_keys,
        separators=separators,
    )


def dump(obj: Any, fh: SupportsWrite[str], **kwargs) -> None:
    fh.write(dumps(obj, **kwargs))
