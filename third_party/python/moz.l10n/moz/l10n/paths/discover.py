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

import re
from collections.abc import Iterable, Iterator
from itertools import chain
from os import sep, walk
from os.path import commonpath, isdir, join, normpath, relpath, splitext

from moz.l10n.resource.format import l10n_extensions
from moz.l10n.util import walk_files

locale_id = re.compile(
    r"[a-z]{2,3}(?:[-_][A-Z][a-z]{3})?(?:[-_][A-Z]{2})?|ca-valencia|ja-JP-mac"
)


def dir_contains(dir: str, path: str) -> bool:
    return commonpath((dir, path)) == dir


def locale_dirname(base: str, locale: str) -> str:
    if "-" in locale and not isdir(join(base, locale)):
        alt_dir = locale.replace("-", "_")
        if isdir(join(base, alt_dir)):
            return alt_dir
    return locale


class MissingSourceDirectoryError(Exception):
    """Raised when no source directory can be found."""


class L10nDiscoverPaths:
    """Automagical localization resource discovery"""

    base: str | None
    """The target base directory, with subdirectories for each locale."""
    locales: list[str] | None
    """Locales detected from subdirectory names under `base`."""
    ref_paths: list[str]
    """Reference paths"""

    def __init__(
        self,
        root: str,
        ref_root: str | None = None,
        *,
        force_paths: list[str] | None = None,
        ignorepath: str | None = ".l10n-ignore",
        source_locale: str | list[str] | None = None,
    ) -> None:
        """
        Automagical localization resource discovery.

        Given a `root` directory, finds the likeliest reference and target directories.

        The reference directory has a name matching the `source_locale`
        and contains files with extensions that appear localizable.

        If `ref_root` is given, the reference directory must be within it.
        If it does not contain a directory matching `source_locale`
        but `ref_root` itself contains files with extensions that appear localizable,
        it is used as the reference directory.

        Use `force_paths` to list fully-qualified file paths to include
        as reference paths if they are within the reference directory,
        even if no file is present at those paths.

        The default `source_locale` is `['en-US', 'en']`,
        with earlier values taking priority.
        For the reference directory, an underscore may also be used as a separator and it may be all lower-case,
        such that a directory named `en_us` is considered to match a source locale `en-US`.

        The localization target root is a directory within `root` with subdirectories named as
        BCP 47 locale identifiers, i.e. like `aa`, `aa-AA`, `aa-Aaaa`, or `aa-Aaaa-AA`.

        To ignore files, include a `.l10n-ignore` file in the reference base directory
        or some other location passed in as `ignorepath`.
        This file uses git-ignore syntax,
        and is always based in the reference base directory.
        """
        root = normpath(root)
        if not isdir(root):
            raise ValueError(f"Not a directory: {root}")
        if ref_root:
            ref_root = normpath(join(root, ref_root))
        if source_locale is None:
            source_locale = ["en-US", "en"]
        elif isinstance(source_locale, str):
            source_locale = [source_locale]
        ref_dir_scores: dict[str, int] = {
            lc_var: idx
            for idx, lc in enumerate(source_locale)
            for lc_var in locale_code_variants(lc)
        }

        # dir -> score
        ref_dirs: dict[str, int] = {ref_root: len(source_locale)} if ref_root else {}
        base_dirs: list[tuple[str, list[str]]] = []  # [(root, [locale_dir])]
        pot_dirs: list[str] = []
        l10n_dirs: list[str] = []
        if ref_root and not dir_contains(root, ref_root):
            walk_roots: Iterator[tuple[str, list[str], list[str]]] = chain(
                walk(root), walk(ref_root)
            )
        else:
            walk_roots = walk(root)
        for dirpath, dirnames, filenames in walk_roots:
            locale_dirs = []
            for dir in dirnames:
                if dir in ref_dir_scores:
                    ref_dirs[join(dirpath, dir)] = ref_dir_scores[dir]
                elif locale_id.fullmatch(dir):
                    locale_dirs.append(dir)
            if locale_dirs:
                base_dirs.append((dirpath, locale_dirs))
            dirnames[:] = (dn for dn in dirnames if not dn.startswith("."))

            if any(not fn.startswith(".") and fn.endswith(".pot") for fn in filenames):
                pot_dirs.append(dirpath)
            if any(
                not fn.startswith(".") and splitext(fn)[1] in l10n_extensions
                for fn in filenames
            ):
                l10n_dirs.append(dirpath)

        if ref_root:
            for dir in list(ref_dirs):
                if not dir_contains(ref_root, dir):
                    del ref_dirs[dir]

        # Filter reference dirs to those with localizable contents,
        # with a preference for .pot template files.
        ref_dirs_with_files = [
            dir for dir in ref_dirs if any(dir_contains(dir, pd) for pd in pot_dirs)
        ] or [dir for dir in ref_dirs if any(dir_contains(dir, ld) for ld in l10n_dirs)]
        if ref_dirs_with_files:
            self._ref_root = min(
                (rd for rd in ref_dirs.items() if rd[0] in ref_dirs_with_files),
                key=lambda s: s[1],
            )[0]
        else:
            raise MissingSourceDirectoryError

        # Pick the localization base dir not in the reference directory
        # with the most locale subdirectories,
        # with a preference for directories with localizable contents.
        base_dirs = [bd for bd in base_dirs if not dir_contains(self._ref_root, bd[0])]
        base_dirs = [
            bd for bd in base_dirs if any(dir_contains(bd[0], ld) for ld in l10n_dirs)
        ] or base_dirs

        locale_dirs_: list[str] | None
        self.base, locale_dirs_ = max(
            base_dirs, key=lambda s: len(s[1]), default=(None, None)
        )
        if locale_dirs_:
            self.locales = [dir.replace("_", "-") for dir in locale_dirs_]
            self.locales.sort()
        else:
            self.locales = None

        self.ref_paths = (
            list(walk_files(self._ref_root, ignorepath=ignorepath))
            if isdir(self._ref_root)
            else []
        )
        if force_paths:
            self.ref_paths.extend(
                path for path in force_paths if dir_contains(self._ref_root, path)
            )

    @property
    def ref_root(self) -> str:
        """The reference root directory."""
        return self._ref_root

    def _base(self) -> str:
        if self.base is None:
            raise ValueError("self.base is required for target paths")
        return self.base

    def all(self) -> dict[tuple[str, str], list[str] | None]:
        """
        Returns a mapping of `(reference_path, target_path)` to `locales`
        for all resources.

        Target paths will include a `{locale}` variable.
        """
        locale_root = join(self._base(), "{locale}")
        paths: dict[tuple[str, str], list[str] | None] = {}
        for ref_path in self.ref_paths:
            target = ref_path.replace(self._ref_root, locale_root, 1)
            if target.endswith(".pot"):
                target = target[:-1]
            paths[(ref_path, target)] = self.locales
        return paths

    def target(
        self, ref_path: str, *, ref_required: bool = True
    ) -> tuple[str | None, Iterable[str]]:
        """
        If `ref_path` is a valid reference path,
        returns its corresponding target path.
        Otherwise, returns `None` for the path.

        Target path will include a `{locale}` variable.
        """
        ref_path = normpath(join(self._ref_root, ref_path))
        if ref_path.endswith(".po"):
            ref_path += "t"
        if ref_required and ref_path not in self.ref_paths:
            return None, ()
        locale_root = join(self._base(), "{locale}")
        target = ref_path.replace(self._ref_root, locale_root, 1)
        if target.endswith(".pot"):
            target = target[:-1]
        return target, self.locales or ()

    def format_target_path(self, target: str, locale: str) -> str:
        base = self._base()
        dir = locale_dirname(base, locale)
        return normpath(join(base, target.format(locale=dir)))

    def find_reference(self, target: str) -> tuple[str, dict[str, str]] | None:
        """
        A reverse lookup for the reference path and locale matching `target`,
        or `None` if not found.

        The locale is returned as `{"locale": locale}` to match `L10nConfig`.
        """
        base = self._base()
        locale, *path_parts = normpath(relpath(join(base, target), base)).split(sep)
        if path_parts and locale_id.fullmatch(locale):
            ref_path = join(self._ref_root, *path_parts)
            vars = {"locale": locale.replace("_", "-")}
            if ref_path in self.ref_paths:
                return ref_path, vars
            elif ref_path.endswith(".po") and ref_path + "t" in self.ref_paths:
                return ref_path + "t", vars
        return None


def locale_code_variants(locale_code: str) -> Iterable[str]:
    yield locale_code
    yield locale_code.lower()
    lc_ = locale_code.replace("-", "_")
    yield lc_
    yield lc_.lower()
