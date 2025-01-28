# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""A generic script to add entries to a file
if the entry does not already exist.

Usage: buildlist.py <filename> <entry> [<entry> ...]
"""

import os.path
import sys

from filelock import SoftFileLock

from mozbuild.dirutils import ensureParentDir


def addEntriesToListFile(listFile, entries):
    """Given a file ``listFile`` containing one entry per line,
    add each entry in ``entries`` to the file, unless it is already
    present."""
    ensureParentDir(listFile)
    with SoftFileLock(listFile + ".lck", timeout=-1):
        if os.path.exists(listFile):
            with open(listFile) as f:
                existing = {x.strip() for x in f.readlines()}
        else:
            existing = set()
        existing.update(entries)
        with open(listFile, "w", newline="\n") as f:
            f.write("\n".join(sorted(existing)) + "\n")


def main(args):
    if len(args) < 2:
        print("Usage: buildlist.py <list file> <entry> [<entry> ...]", file=sys.stderr)
        return 1

    return addEntriesToListFile(args[0], args[1:])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
