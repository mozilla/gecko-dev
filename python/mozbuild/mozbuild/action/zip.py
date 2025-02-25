# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script creates a zip file, but will also strip any binaries
# it finds before adding them to the zip.

import argparse
import os
import sys

import mozpack.path as mozpath
from mozpack.copier import Jarrer
from mozpack.errors import errors
from mozpack.files import FileFinder
from mozpack.path import match

from mozbuild.makeutil import Makefile
from mozbuild.util import FileAvoidWrite


def main(args):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-C",
        metavar="DIR",
        default=".",
        help="Change to given directory before considering " "other paths",
    )
    parser.add_argument("--strip", action="store_true", help="Strip executables")
    parser.add_argument(
        "-x",
        metavar="EXCLUDE",
        default=[],
        action="append",
        help="Exclude files that match the pattern",
    )
    parser.add_argument("zip", help="Path to zip file to write")
    parser.add_argument("input", nargs="+", help="Path to files to add to zip")
    parser.add_argument(
        "--dep-file",
        help="File to write any additional make dependencies to",
    )
    parser.add_argument(
        "--dep-target",
        help="Make target to use in the dependencies file",
    )
    args = parser.parse_args(args)

    jarrer = Jarrer()

    deps = []
    with errors.accumulate():
        finder = FileFinder(args.C, find_executables=args.strip)
        for path in args.input:
            for p, f in finder.find(path):
                if not any([match(p, exclude) for exclude in args.x]):
                    jarrer.add(p, f)
                    deps.append(f.path)
        jarrer.copy(mozpath.join(args.C, args.zip))

    if args.dep_target and args.dep_file:
        mk = Makefile()
        mk.create_rule([args.dep_target]).add_dependencies(deps)
        mk.create_rule(deps)  # empty rule to avoid error if a file gets removed

        os.makedirs(mozpath.dirname(args.dep_file), exist_ok=True)
        with FileAvoidWrite(args.dep_file) as dep_file:
            mk.dump(dep_file)


if __name__ == "__main__":
    main(sys.argv[1:])
