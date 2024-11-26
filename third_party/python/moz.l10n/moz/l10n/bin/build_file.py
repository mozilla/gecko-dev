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
from argparse import ArgumentParser, RawDescriptionHelpFormatter
from os import makedirs
from os.path import dirname, exists
from shutil import copyfile
from textwrap import dedent

from moz.l10n.bin.build import write_target_file
from moz.l10n.resource import UnsupportedResource, parse_resource

log = logging.getLogger(__name__)


def cli() -> None:
    parser = ArgumentParser(
        description=dedent(
            """
            Build one localization file for release.

            Uses the --source file as a baseline, applying --l10n localizations to build --target.

            Trims out all comments and messages not in the source file.
            """
        ),
        formatter_class=RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-v", "--verbose", action="count", default=0, help="increase logging verbosity"
    )
    parser.add_argument("--source", metavar="PATH", required=True, help="source file")
    parser.add_argument(
        "--l10n", metavar="PATH", required=True, help="localization file"
    )
    parser.add_argument("--target", metavar="PATH", required=True, help="output target")
    args = parser.parse_args()

    log_level = (
        logging.WARNING
        if args.verbose == 0
        else logging.INFO
        if args.verbose == 1
        else logging.DEBUG
    )
    logging.basicConfig(format="%(message)s", level=log_level)

    try:
        try:
            source_res = parse_resource(args.source)
        except UnsupportedResource:
            source_res = None
        makedirs(dirname(args.target), exist_ok=True)
        if source_res is None:
            from_path = args.l10n if exists(args.l10n) else args.source
            copyfile(from_path, args.target)
        else:
            write_target_file(args.source, source_res, args.l10n, args.target)
    except (OSError, UnsupportedResource) as error:
        raise SystemExit(error)


if __name__ == "__main__":
    cli()
