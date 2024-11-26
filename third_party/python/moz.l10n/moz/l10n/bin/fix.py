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

import logging
import sys
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from collections.abc import Iterable
from enum import Enum
from glob import glob
from os import getcwd
from os.path import abspath, isdir, relpath
from textwrap import dedent

from moz.l10n.paths.config import L10nConfigPaths
from moz.l10n.paths.discover import L10nDiscoverPaths
from moz.l10n.resource import UnsupportedResource, parse_resource, serialize_resource

log = logging.getLogger(__name__)

Result = Enum("Result", ("OK", "FIXED", "UNSUPPORTED", "FAIL"))


def cli() -> None:
    parser = ArgumentParser(
        description=dedent(
            """
            Fix the formatting for localization resources.

            If `paths` is a single directory, it is iterated with L10nConfigPaths if --config is set, or L10nDiscoverPaths otherwise.

            If `paths` is not a single directory, its values are treated as glob expressions, with ** support.
            """
        ),
        formatter_class=RawDescriptionHelpFormatter,
    )
    parser.add_argument("-q", "--quiet", action="store_true", help="only log errors")
    parser.add_argument(
        "-v", "--verbose", action="count", default=0, help="increase logging verbosity"
    )
    parser.add_argument(
        "--config", metavar="PATH", type=str, help="path to l10n.toml config file"
    )
    parser.add_argument(
        "--continue",
        action="store_true",
        dest="continue_on_error",
        help="do not stop at first parse error",
    )
    parser.add_argument("paths", nargs="*", type=str, help="directory or files to fix")
    args = parser.parse_args()

    log_level = (
        logging.ERROR
        if args.quiet
        else (
            logging.WARNING
            if args.verbose == 0
            else logging.INFO
            if args.verbose == 1
            else logging.DEBUG
        )
    )
    logging.basicConfig(format="%(message)s", level=log_level)

    res = fix(args.paths, args.config, args.continue_on_error)
    sys.exit(res)


def fix(
    file_paths: list[str],
    config_path: str | None = None,
    continue_on_error: bool = False,
) -> int:
    """
    Fix the formatting for `file_paths` localization resources.

    If a single directory is given,
    it is iterated with `L10nConfigPaths` if `config_path` is set,
    or `L10nDiscoverPaths` otherwise.

    If `file_paths` is not a single directory,
    the paths are treated as glob expressions.

    If `continue_on_error` is not set, operation terminates on the first error.

    Returns 0 on success, 1 on parse error, or 2 on argument error.
    """
    if config_path:
        if file_paths:
            log.error("With --config, paths must not be set.")
            return 2
        cfg_paths = L10nConfigPaths(config_path)
        root_dir = abspath(cfg_paths.base)
        path_iter: Iterable[str] = cfg_paths.ref_paths
    elif len(file_paths) == 1 and isdir(file_paths[0]):
        root_dir = abspath(file_paths[0])
        path_iter = L10nDiscoverPaths(root_dir, ref_root=".").ref_paths
    elif file_paths:
        root_dir = getcwd()
        path_iter = (path for fp in file_paths for path in glob(fp, recursive=True))
    else:
        log.error("Either paths of --config is required")
        return 2

    fixed = 0
    unsupported = 0
    failed = 0
    total = 0
    for path in path_iter:
        res = fix_file(root_dir, path)
        total += 1
        if res == Result.FIXED:
            fixed += 1
        elif res == Result.UNSUPPORTED:
            unsupported += 1
        elif res == Result.FAIL:
            failed += 1
            if not continue_on_error:
                break

    log.warning("")
    if unsupported > 0:
        log.warning(plural(f"Skipped {unsupported} unsupported file", unsupported))
    touched = total - unsupported
    if touched == 0:
        log.warning("Found no localization resources")
    else:
        log.warning(plural(f"Fixed {fixed}/{touched} file", touched))
    if failed > 0:
        log.warning(plural(f"With {failed} parse failure", failed))
    return 0 if failed == 0 else 1


def fix_file(root: str, path: str) -> Result:
    rel_path = relpath(path, root)
    try:
        with open(path, "+rb") as file:
            prev = file.read()
            res = parse_resource(path, prev)
            next = bytearray()
            for line in serialize_resource(res):
                next.extend(line.encode("utf-8"))
            if next == prev:
                log.info(f"OK: {rel_path}")
                return Result.OK
            else:
                file.seek(0)
                file.write(next)
                file.truncate()
                log.warning(f"Fixed: {rel_path}")
                return Result.FIXED
    except (UnsupportedResource, UnicodeDecodeError):
        log.info(f"Skip: {rel_path}")
        return Result.UNSUPPORTED
    except Exception as error:
        log.error(f"FAIL: {rel_path}\n{error}")
        return Result.FAIL


def plural(noun: str, count: int) -> str:
    """Hacky and English-only"""
    return noun if count == 1 else noun + "s"


if __name__ == "__main__":
    cli()
