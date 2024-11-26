#!/usr/bin/env python

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

import json
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from os.path import abspath, basename, dirname, isdir, join, normpath, relpath
from textwrap import dedent

from moz.l10n.paths import L10nConfigPaths, L10nDiscoverPaths
from moz.l10n.resource import UnsupportedResource, parse_resource
from moz.l10n.resource.data import Entry


def cli() -> None:
    parser = ArgumentParser(
        description=dedent(
            """
            Compare localizations to their `source`, which may be
            - a directory (using L10nDiscoverPaths),
            - a TOML config file (using L10nConfigPaths), or
            - a JSON file containing a mapping of file paths to arrays of messages.
            """
        ),
        formatter_class=RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-v", "--verbose", action="count", default=0, help="increase output verbosity"
    )
    parser.add_argument("--json", action="store_true", help="output JSON")
    parser.add_argument(
        "--ext", nargs="+", type=str, help="file extensions, prefix with ! to exclude"
    )
    parser.add_argument(
        "--source",
        metavar="PATH",
        required=True,
        type=str,
        help="path to source file listing expected files & messages",
    )
    parser.add_argument("paths", nargs="+", type=str, help="directories to test")
    args = parser.parse_args()

    ext_include: set[str] = set()
    ext_exclude: set[str] = set()
    if args.ext:
        arg_ext: list[str] = args.ext
        if len(arg_ext) == 1 and "," in arg_ext[0]:
            arg_ext = [ext.strip() for ext in arg_ext[0].split(",")]
        for ext in arg_ext:
            if ext.startswith("!"):
                ext = ext[1:]
                ext_exclude.add(ext if ext.startswith(".") else f".{ext}")
            else:
                ext_include.add(ext if ext.startswith(".") else f".{ext}")

    def ext_filter(path: str) -> bool:
        included = not ext_include or any(path.endswith(ext) for ext in ext_include)
        excluded = ext_exclude and any(path.endswith(ext) for ext in ext_exclude)
        return included and not excluded

    if args.source.endswith(".json"):
        with open(args.source) as f:
            source_data: dict[str, list[str] | set[str]] = json.load(f)
        if ext_include or ext_exclude:
            source_data = {k: set(v) for k, v in source_data.items() if ext_filter(k)}
    else:
        source_paths: L10nConfigPaths | L10nDiscoverPaths = (
            L10nConfigPaths(args.source)
            if args.source.endswith(".toml")
            else L10nDiscoverPaths(args.source, args.source)
        )
        path0 = abspath(args.paths[0])
        locale0 = basename(path0)
        source_paths.base = dirname(path0)
        source_data = {}
        for ref_path, tgt_path in source_paths.all():
            if ext_filter(tgt_path):
                try:
                    path = relpath(tgt_path.format(locale=locale0), path0)
                    source_data[path] = msg_ids(ref_path)
                except UnsupportedResource:
                    continue
    source_total = sum(len(sd) for sd in source_data.values())
    if source_total == 0:
        raise ValueError(f"No messages found for source {args.source}")
    if not args.json:
        print(f"source: {source_total}")

    json_res = {}
    for path in args.paths:
        if not isdir(path):
            continue
        lc = basename(normpath(path))
        errors, missing = compare(source_data, path)
        if args.json:
            json_res[lc] = {
                "errors": errors or None,
                "missing": missing or None,
            }
        else:
            total = sum(len(rm) for rm in missing.values())
            print(f"{lc}: {-total}")
            for path, error in errors.items():
                print(f"  !!! {path}: {error}")
            if args.verbose > 0:
                for path, messages in missing.items():
                    print(f"  {path}: {-len(messages)}")
                    if args.verbose > 1:
                        for msg in messages:
                            print(f"    {msg}")

    if args.json:
        print(json.dumps(json_res))


def compare(
    source_data: dict[str, list[str] | set[str]], root: str
) -> tuple[dict[str, str], dict[str, list[str]]]:
    errors: dict[str, str] = {}
    missing: dict[str, list[str]] = {}
    for path, src_messages in source_data.items():
        if src_messages:
            try:
                tgt_messages = msg_ids(join(root, path))
                for msg in src_messages:
                    if msg not in tgt_messages:
                        if path in missing:
                            missing[path].append(msg)
                        else:
                            missing[path] = [msg]
            except FileNotFoundError:
                missing[path] = list(src_messages)
            except Exception as e:
                errors[path] = str(e)
    return errors, missing


def msg_ids(path: str) -> set[str]:
    res = parse_resource(path)
    return {
        ".".join(section.id + entry.id)
        for section in res.sections
        for entry in section.entries
        if isinstance(entry, Entry)
    }


if __name__ == "__main__":
    cli()
