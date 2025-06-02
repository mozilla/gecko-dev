# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from typing import Any, Dict, Generator, Tuple

from taskgraph.util.attributes import keymatch


def iter_dot_path(
    container: Dict[str, Any], subfield: str
) -> Generator[Tuple[Dict[str, Any], str], None, None]:
    """Given a container and a subfield in dot path notation, yield the parent
    container of the dotpath's leaf node, along with the leaf node name that it
    contains.

    If the dot path contains a list object, each item in the list will be
    yielded.

    Args:
        container (dict): The container to search for the dot path.
        subfield (str): The dot path to search for.
    """
    while "." in subfield:
        f, subfield = subfield.split(".", 1)

        if f.endswith("[]"):
            f = f[0:-2]
            if not isinstance(container.get(f), list):
                return

            for item in container[f]:
                yield from iter_dot_path(item, subfield)
            return

        if f not in container:
            return

        container = container[f]

    if isinstance(container, dict) and subfield in container:
        yield container, subfield


def evaluate_keyed_by(
    value, item_name, attributes, defer=None, enforce_single_match=True
):
    """
    For values which can either accept a literal value, or be keyed by some
    attributes, perform that lookup and return the result.

    For example, given item::

        by-test-platform:
            macosx-10.11/debug: 13
            win.*: 6
            default: 12

    a call to `evaluate_keyed_by(item, 'thing-name', {'test-platform': 'linux96')`
    would return `12`.

    Items can be nested as deeply as desired::

        by-test-platform:
            win.*:
                by-project:
                    ash: ..
                    cedar: ..
            linux: 13
            default: 12

    Args:
        value (str): Name of the value to perform evaluation on.
        item_name (str): Used to generate useful error messages.
        attributes (dict): Dictionary of attributes used to lookup 'by-<key>' with.
        defer (list):
            Allows evaluating a by-* entry at a later time. In the example
            above it's possible that the project attribute hasn't been set yet,
            in which case we'd want to stop before resolving that subkey and
            then call this function again later. This can be accomplished by
            setting `defer=["project"]` in this example.
        enforce_single_match (bool):
            If True (default), each task may only match a single arm of the
            evaluation.
    """
    while True:
        if not isinstance(value, dict) or len(value) != 1:
            return value
        value_key = next(iter(value))
        if not value_key.startswith("by-"):
            return value

        keyed_by = value_key[3:]  # strip off 'by-' prefix

        if defer and keyed_by in defer:
            return value

        key = attributes.get(keyed_by)
        alternatives = next(iter(value.values()))

        if len(alternatives) == 1 and "default" in alternatives:
            # Error out when only 'default' is specified as only alternatives,
            # because we don't need to by-{keyed_by} there.
            raise Exception(
                f"Keyed-by '{keyed_by}' unnecessary with only value 'default' "
                f"found, when determining item {item_name}"
            )

        if key is None:
            if "default" in alternatives:
                value = alternatives["default"]
                continue
            else:
                raise Exception(
                    f"No attribute {keyed_by} and no value for 'default' found "
                    f"while determining item {item_name}"
                )

        matches = keymatch(alternatives, key)
        if enforce_single_match and len(matches) > 1:
            raise Exception(
                f"Multiple matching values for {keyed_by} {key!r} found while "
                f"determining item {item_name}"
            )
        elif matches:
            value = matches[0]
            continue

        raise Exception(
            f"No {keyed_by} matching {key!r} nor 'default' found while determining item {item_name}"
        )
