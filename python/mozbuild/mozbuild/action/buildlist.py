# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""A generic script to add entries to a file
if the entry does not already exist.

Usage: buildlist.py <filename> <entry> [<entry> ...]
"""
import os.path
import sys

from mozbuild.dirutils import ensureParentDir
from mozbuild.lock import lock_file


def addEntriesToListFile(listFile, entries):
    """Given a file ``listFile`` containing one entry per line,
    add each entry in ``entries`` to the file, unless it is already
    present."""
    ensureParentDir(listFile)
    lock = lock_file(listFile + ".lck")
    try:
        if os.path.exists(listFile):
            with open(listFile) as f:
                existing = {x.strip() for x in f.readlines()}
        else:
            existing = set()
        existing.update(entries)
        with open(listFile, "w", newline="\n") as f:
            f.write("\n".join(sorted(existing)) + "\n")
    finally:
        del lock  # Explicitly release the lock_file to free it


def main(args):
    if len(args) < 2:
        print("Usage: buildlist.py <list file> <entry> [<entry> ...]", file=sys.stderr)
        return 1

    return addEntriesToListFile(args[0], args[1:])


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
