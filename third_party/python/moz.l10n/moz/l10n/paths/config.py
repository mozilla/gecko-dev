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

import sys
from collections.abc import Callable, Iterable, Iterator
from glob import glob
from os import sep
from os.path import dirname, isfile, join, normpath, relpath
from re import Pattern, compile
from typing import Any, Dict

if sys.version_info >= (3, 11):
    from tomllib import load
else:
    from tomli import load

path_stars = compile(r"[*](?:[*](?:[/\\][*]*)?)?")
path_var = compile(r"{(\w+)}")


def path_regex(path: str) -> Pattern[str]:
    """
    Captures * groups as indexed and {vars} as named.
    Expects `path` to use `/` as separator.
    """
    if path.startswith("{l10n_base}/"):
        path = path[12:]
    path = path_stars.sub(
        lambda m: (
            "([^/]*)" if m[0] == "*" else "((?:.*/)?)" if m[0] == "**/" else "(.*)"
        ),
        path,
    )
    path = path_var.sub(r"(?P<\1>[^/]*)", path)
    return compile(path)


class PartialMap(Dict[str, str]):
    """Allows `str.format_map()` calls with partial values."""

    def __missing__(self, key: str) -> str:
        return "{" + str(key) + "}"


class L10nConfigPaths:
    """
    Wrapper for localization config files.

    Supports a subset of the format specified at:
    https://moz-l10n-config.readthedocs.io/en/latest/fileformat.html

    Differences:
    - `[build]` is ignored
    - `[[excludes]]` are not supported
    - `[[filters]]` are ignored
    - `[[paths]]` must always include both `reference` and `l10n`

    Does not consider `.l10n-ignore` files.
    """

    def __init__(
        self,
        cfg_path: str,
        *,
        cfg_load: Callable[[str], dict[str, Any]] | None = None,
        force_paths: list[str] | None = None,
        locale_map: dict[str, Callable[[str], str]] | None = None,
        _seen: set[str] | None = None,
    ) -> None:
        """
        To customize the loading of a configuration at `cfg_path`, set `cfg_load`.

        As configurations may include others, `cfg_load` can get called multiple times.
        `_seen` is used internally to deduplicate file loads.

        Use `force_paths` to list fully-qualified file paths to include
        as reference paths if they match the `[[paths]]` config,
        even if no file is present at those paths.

        To use custom path variables for locales,
        set `locale_map` to be a mapping of path variable names to functions,
        which will be called with `locale` as their only argument.
        """
        if cfg_load:
            toml = cfg_load(cfg_path)
        else:
            with open(cfg_path, mode="rb") as file:
                toml = load(file)
        self._cfg_path = cfg_path
        self._locale_map = locale_map or {}
        base = toml.get("basepath", ".")
        self._base = normpath(join(dirname(cfg_path), base))
        self._ref_root = self._base
        self._locales: list[str] | None = toml.get("locales", None)
        env = toml.get("env", None)
        env_map = PartialMap(env) if env else None

        self._templates: list[tuple[str, Pattern[str]]] = []  #
        """
        `[(ref, target)]`

        To find references for targets,
        retains a `ref` string with `{}` slots for the corresponding
        `*` and `**` parts of the template paths,
        which are also the indexed groups captured in `target`.
        """

        self._path_data: dict[str, tuple[str, list[str] | None]] = {}
        """ ref -> (target, locales) """
        fp = set(force_paths) if force_paths else None
        for path in toml.get("paths", []):
            ref: str = normpath(join(self._ref_root, path["reference"]))
            target: str = path["l10n"]  # Note: not normalised, so sep=="/"
            if env_map:
                target = target.format_map(env_map)
            self._templates.append((path_stars.sub("{}", ref), path_regex(target)))
            locales: list[str] | None = path.get("locales", None)
            if "*" in ref:
                if ref.count("*") != target.count("*"):
                    raise ValueError(
                        f"Wildcard mismatch between reference & l10n: {path}"
                    )
                ref_re = compile(
                    path_stars.sub("(.*)", ref.replace(sep, "/").replace(".", r"\."))
                )
                *tgt_parts, tgt_end = path_stars.split(target)

                def get_target(ref_file: str) -> str:
                    m = ref_re.fullmatch(ref_file.replace(sep, "/"))
                    assert m is not None, f"Unexpected ref with path {path}"
                    return (
                        "".join(a + b for a, b in zip(tgt_parts, m.groups())) + tgt_end
                    )

                self._path_data.update(
                    (ref_file, (get_target(ref_file), locales))
                    for ref_file in glob(ref, recursive=True)
                    if isfile(ref_file)
                )
                if fp:
                    ref_re = path_regex(ref.replace(sep, "/"))
                    fp_match = {
                        path for path in fp if ref_re.fullmatch(path.replace(sep, "/"))
                    }
                    if fp_match:
                        self._path_data.update(
                            (path, (get_target(path), locales)) for path in fp_match
                        )
                        fp -= fp_match
            else:
                self._path_data[ref] = (target, locales)

        self._includes: list[L10nConfigPaths] = []
        if "includes" in toml:
            if _seen is None:
                _seen = set()
            for incl in toml["includes"]:
                incl_path: str = incl["path"]
                if env_map:
                    incl_path = incl_path.format_map(env_map)
                incl_path = normpath(join(self._ref_root, incl_path))
                if incl_path not in _seen:
                    _seen.add(incl_path)
                    self._includes.append(
                        L10nConfigPaths(incl_path, cfg_load=cfg_load, _seen=_seen)
                    )

    @property
    def base(self) -> str:
        """
        The configuration root,
        determined in the TOML by `basepath` relative to the config file path
        or set by the user.
        """
        return self._base

    @base.setter
    def base(self, base: str) -> None:
        for incl in self._includes:
            incl.base = base
        self._base = base

    @property
    def locales(self) -> list[str] | None:
        """
        Locales for the config,
        determined in the TOML by `locales` or set directly by the user.
        """
        return self._locales

    @locales.setter
    def locales(self, locales: list[str] | None) -> None:
        self._locales = locales
        for incl in self._includes:
            incl.locales = locales

    @property
    def ref_root(self) -> str:
        """The reference root directory."""
        return self._ref_root

    @property
    def ref_paths(self) -> Iterator[str]:
        yield from self._path_data
        for incl in self._includes:
            yield from incl.ref_paths

    def config_paths(self) -> Iterator[str]:
        yield self._cfg_path
        for incl in self._includes:
            yield from incl.config_paths()

    def all(
        self, format_map: dict[str, str] | None = None
    ) -> dict[tuple[str, str], list[str] | None]:
        """
        Returns a mapping of `(reference_path, target_path)` to `locales`
        for all resources.

        In target paths, `{l10n_base}` is replaced by `self.base`.
        Any `{locale}` or `locale_map` variables will be left in.
        Additional format variables may be set in `format_map`.
        """
        all: dict[tuple[str, str], list[str] | None] = {}
        for key, locales in self._all(format_map):
            prev = all.get(key, None)
            if prev is None:
                all[key] = locales
            elif locales:
                locales_ = list(set(prev).union(locales))
                locales_.sort()
                all[key] = locales_
        return all

    def _all(
        self, format_map: dict[str, str] | None
    ) -> Iterator[tuple[tuple[str, str], list[str] | None]]:
        lc_map = PartialMap(format_map or ())
        lc_map["l10n_base"] = self._base
        for ref, (target, locales) in self._path_data.items():
            target = target.format_map(lc_map)
            if target.endswith(".pot"):
                target = target[:-1]
            target = normpath(join(self._base, target))
            yield (ref, target), locales or self._locales
        for incl in self._includes:
            yield from incl._all(format_map)

    def target(
        self,
        ref_path: str,
        *,
        format_map: dict[str, str] | None = None,
    ) -> tuple[str | None, Iterable[str]]:
        """
        If `ref_path` is a valid reference path,
        returns its corresponding target path and locales.
        Otherwise, returns `None` for the path.

        In the target path, `{l10n_base}` is replaced by `self.base`.
        Any `{locale}` or `locale_map` variables will be left in.
        Additional format variables may be set in `format_map`.
        """
        norm_ref_path = normpath(join(self._ref_root, ref_path))
        if norm_ref_path.endswith(".po"):
            norm_ref_path += "t"
        pd = self._path_data.get(norm_ref_path, None)
        if pd is None:
            for incl in self._includes:
                target = incl.target(norm_ref_path, format_map=format_map)
                if target[0] is not None:
                    return target
            return None, ()
        pd_path, pd_locales = pd

        fmt_map = PartialMap(format_map or ())
        fmt_map["l10n_base"] = self._base
        path = pd_path.format_map(fmt_map)
        if path.endswith(".pot"):
            path = path[:-1]
        path = normpath(join(self._base, path))

        locales = (
            set(pd_locales).intersection(self._locales)
            if pd_locales and self._locales
            else pd_locales or self._locales or ()
        )

        return path, locales

    def format_target_path(self, target: str, locale: str) -> str:
        lc_map = {"locale": locale}
        for key, fn in self._locale_map.items():
            lc_map[key] = fn(locale)
        return normpath(join(self._base, target.format_map(lc_map)))

    def find_reference(self, target: str) -> tuple[str, dict[str, str]] | None:
        """
        A reverse lookup for the reference path and variables matching `target`,
        or `None` if not found.
        """
        abs_target = join(self._base, normpath(target))
        rel_target = normpath(relpath(abs_target, self._base)).replace(sep, "/")
        for ref, pattern in self._templates:
            match = pattern.fullmatch(rel_target)
            if match:
                vars = match.groupdict()
                var_spans = {match.span(name) for name in vars}
                star_values = [
                    group
                    for idx, group in enumerate(match.groups())
                    if match.span(idx + 1) not in var_spans
                ]
                ref_path = normpath(ref.format(*star_values))
                if ref_path in self._path_data:
                    return ref_path, vars
                elif ref_path.endswith(".po") and ref_path + "t" in self._path_data:
                    return ref_path + "t", vars
        for incl in self._includes:
            res = incl.find_reference(abs_target)
            if res is not None:
                return res
        return None
