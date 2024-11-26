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

from collections.abc import Iterator
from os import sep, walk
from os.path import isdir, join, relpath

from gitignorant import Rule, check_match, parse_gitignore_file


def walk_files(
    root: str, dirs: list[str] | None = None, ignorepath: str | None = ".l10n-ignore"
) -> Iterator[str]:
    """
    Iterate through all files under the `root` directory.
    Use `dirs` to limit the search to only some subdirectories under `root`.

    All files and directories with names starting with `.` are ignored.
    To ignore other files, include a `.l10n-ignore` file in `root`,
    or some other location passed in as `ignorepath`.
    This file uses git-ignore syntax,
    and is always based in the `root` directory.
    """
    if not isdir(root):
        raise ValueError(f"Not a directory: {root}")
    ignore = [Rule(negative=False, content=".*")]
    if ignorepath:
        try:
            with open(join(root, ignorepath), encoding="utf-8") as file:
                ignore += parse_gitignore_file(file)
        except OSError:
            pass
    for dir in (join(root, p) for p in dirs) if dirs else (root,):
        for dirpath, dirnames, filenames in walk(dir):
            idx = len(dirnames) - 1
            while idx >= 0:
                rp = relpath(join(dirpath, dirnames[idx]), start=root)
                if sep != "/":
                    rp = rp.replace(sep, "/")
                if check_match(ignore, rp, is_dir=True):
                    del dirnames[idx]
                idx -= 1
            for fn in filenames:
                path = join(dirpath, fn)
                rp = relpath(path, start=root)
                if sep != "/":
                    rp = rp.replace(sep, "/")
                if check_match(ignore, rp):
                    continue
                yield path
