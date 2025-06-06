# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import collections
import pprint
import re

import voluptuous

import taskgraph
from taskgraph.util.keyed_by import evaluate_keyed_by, iter_dot_path


def validate_schema(schema, obj, msg_prefix):
    """
    Validate that object satisfies schema.  If not, generate a useful exception
    beginning with msg_prefix.
    """
    if taskgraph.fast:
        return
    try:
        schema(obj)
    except voluptuous.MultipleInvalid as exc:
        msg = [msg_prefix]
        for error in exc.errors:
            msg.append(str(error))
        raise Exception("\n".join(msg) + "\n" + pprint.pformat(obj))


def optionally_keyed_by(*arguments):
    """
    Mark a schema value as optionally keyed by any of a number of fields.  The
    schema is the last argument, and the remaining fields are taken to be the
    field names.  For example:

        'some-value': optionally_keyed_by(
            'test-platform', 'build-platform',
            Any('a', 'b', 'c'))

    The resulting schema will allow nesting of `by-test-platform` and
    `by-build-platform` in either order.
    """
    schema = arguments[-1]
    fields = arguments[:-1]

    def validator(obj):
        if isinstance(obj, dict) and len(obj) == 1:
            k, v = list(obj.items())[0]
            if k.startswith("by-") and k[len("by-") :] in fields:
                res = {}
                for kk, vv in v.items():
                    try:
                        res[kk] = validator(vv)
                    except voluptuous.Invalid as e:
                        e.prepend([k, kk])
                        raise
                return res
        return Schema(schema)(obj)

    return validator


def resolve_keyed_by(
    item, field, item_name, defer=None, enforce_single_match=True, **extra_values
):
    """
    For values which can either accept a literal value, or be keyed by some
    other attribute of the item, perform that lookup and replacement in-place
    (modifying `item` directly).  The field is specified using dotted notation
    to traverse dictionaries.

    For example, given item::

        task:
            test-platform: linux128
            chunks:
                by-test-platform:
                    macosx-10.11/debug: 13
                    win.*: 6
                    default: 12

    a call to `resolve_keyed_by(item, 'task.chunks', item['thing-name'])`
    would mutate item in-place to::

        task:
            test-platform: linux128
            chunks: 12

    The `item_name` parameter is used to generate useful error messages.

    If extra_values are supplied, they represent additional values available
    for reference from by-<field>.

    Items can be nested as deeply as the schema will allow::

        chunks:
            by-test-platform:
                win.*:
                    by-project:
                        ash: ..
                        cedar: ..
                linux: 13
                default: 12

    Args:
        item (dict): Object being evaluated.
        field (str): Name of the key to perform evaluation on.
        item_name (str): Used to generate useful error messages.
        defer (list):
            Allows evaluating a by-* entry at a later time. In the example
            above it's possible that the project attribute hasn't been set yet,
            in which case we'd want to stop before resolving that subkey and
            then call this function again later. This can be accomplished by
            setting `defer=["project"]` in this example.
        enforce_single_match (bool):
            If True (default), each task may only match a single arm of the
            evaluation.
        extra_values (kwargs):
            If supplied, represent additional values available
            for reference from by-<field>.

    Returns:
        dict: item which has also been modified in-place.
    """
    for container, subfield in iter_dot_path(item, field):
        container[subfield] = evaluate_keyed_by(
            value=container[subfield],
            item_name=f"`{field}` in `{item_name}`",
            defer=defer,
            enforce_single_match=enforce_single_match,
            attributes=dict(item, **extra_values),
        )

    return item


# Schemas for YAML files should use dashed identifiers by default.  If there are
# components of the schema for which there is a good reason to use another format,
# they can be excepted here.
EXCEPTED_SCHEMA_IDENTIFIERS = [
    # upstream-artifacts and artifact-map are handed directly to scriptWorker,
    # which expects interCaps
    "upstream-artifacts",
    "artifact-map",
]


def check_schema(schema):
    identifier_re = re.compile(r"^\$?[a-z][a-z0-9-]*$")

    def excepted(item):
        for esi in EXCEPTED_SCHEMA_IDENTIFIERS:
            if isinstance(esi, str):
                if f"[{esi!r}]" in item:
                    return True
            elif esi(item):
                return True
        return False

    def iter(path, sch):
        def check_identifier(path, k):
            if k in (str,) or k in (str, voluptuous.Extra):
                pass
            elif isinstance(k, voluptuous.NotIn):
                pass
            elif isinstance(k, str):
                if not identifier_re.match(k) and not excepted(path):
                    raise RuntimeError(
                        "YAML schemas should use dashed lower-case identifiers, "
                        f"not {k!r} @ {path}"
                    )
            elif isinstance(k, (voluptuous.Optional, voluptuous.Required)):
                check_identifier(path, k.schema)
            elif isinstance(k, (voluptuous.Any, voluptuous.All)):
                for v in k.validators:
                    check_identifier(path, v)
            elif not excepted(path):
                raise RuntimeError(
                    f"Unexpected type in YAML schema: {type(k).__name__} @ {path}"
                )

        if isinstance(sch, collections.abc.Mapping):  # type: ignore
            for k, v in sch.items():
                child = f"{path}[{k!r}]"
                check_identifier(child, k)
                iter(child, v)
        elif isinstance(sch, (list, tuple)):
            for i, v in enumerate(sch):
                iter(f"{path}[{i}]", v)
        elif isinstance(sch, voluptuous.Any):
            for v in sch.validators:
                iter(path, v)

    iter("schema", schema.schema)


class Schema(voluptuous.Schema):
    """
    Operates identically to voluptuous.Schema, but applying some taskgraph-specific checks
    in the process.
    """

    def __init__(self, *args, check=True, **kwargs):
        super().__init__(*args, **kwargs)

        self.check = check
        if not taskgraph.fast and self.check:
            check_schema(self)

    def extend(self, *args, **kwargs):
        schema = super().extend(*args, **kwargs)

        if self.check:
            check_schema(schema)
        # We want twice extend schema to be checked too.
        schema.__class__ = Schema
        return schema

    def _compile(self, schema):
        if taskgraph.fast:
            return
        return super()._compile(schema)

    def __getitem__(self, item):
        return self.schema[item]  # type: ignore


OptimizationSchema = voluptuous.Any(
    # always run this task (default)
    None,
    # search the index for the given index namespaces, and replace this task if found
    # the search occurs in order, with the first match winning
    {"index-search": [str]},
    # skip this task if none of the given file patterns match
    {"skip-unless-changed": [str]},
)

# shortcut for a string where task references are allowed
taskref_or_string = voluptuous.Any(
    str,
    {voluptuous.Required("task-reference"): str},
    {voluptuous.Required("artifact-reference"): str},
)
