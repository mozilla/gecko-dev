# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

# Skip locking for packages or specific versions that aren’t published on PyPI
SKIP_LIBS = ["dlmanager", "gyp", "html5lib", "wptrunner", "webdriver"]
# Directories with these suffixes hold only package code; look in their parent for setup.py/metadata
UNWRAP_DIRS = ["src", "lib", "pylib"]


@dataclass
class Dependency:
    name: str
    version: str
    path: Optional[str]


class SiteFileNotFoundError(Exception):
    pass


class DependencyParseError(Exception):
    pass


class SiteDependencyExtractor:

    def __init__(self, site_name: str, sites_dir: Path, topsrcdir: Path) -> None:
        self.site_file = sites_dir / f"{site_name}.txt"
        if not self.site_file.is_file():
            raise SiteFileNotFoundError(f"Site file not found: {self.site_file}")

        self.topsrcdir = topsrcdir
        self.requires_python = None
        self.dependencies: list[Dependency] = []

    def parse(self) -> tuple[Optional[str], list[Dependency]]:
        handlers = {
            "requires-python": self._handle_requires_python,
            "pth": self._handle_pth,
            "pypi": self._handle_pypi,
            "pypi-optional": self._handle_pypi,
            "vendored": self._handle_vendored,
            "vendored-fallback": self._handle_vendored_fallback,
        }

        for raw in self.site_file.read_text().splitlines():
            line = raw.strip()
            if not line or line.startswith("#"):
                continue

            key, _, rest = line.partition(":")
            rest = rest.strip()
            handler = handlers.get(key.lower())
            if handler:
                handler(rest)

        return self.requires_python, self.dependencies

    def _handle_requires_python(self, rest: str) -> None:
        self.requires_python = rest.strip()

    def _handle_pth(selfself, rest: str) -> None:
        # pth dependencies that cannot be locked
        pass

    def _handle_vendored_fallback(self, rest: str) -> None:
        path = rest.split(":", 2)[1]
        self._handle_vendored(path)

    def _handle_vendored(self, rest: str) -> None:
        relative_path = Path(rest)
        if relative_path.name in UNWRAP_DIRS:
            relative_path = relative_path.parent

        name = relative_path.name

        if name in SKIP_LIBS:
            return

        source_path = self.topsrcdir / relative_path

        if not source_path.exists():
            raise DependencyParseError(
                f"\nDirectory not found: {source_path}.\nThis likely indicates that the "
                "dependency was removed manually or implicitly during python vendoring.\n"
                "Review the UV log above and make the necessary updates to the all <site>.txt files."
            )

        version_providers = (
            self._version_from_metadata_files,
            self._version_from_setup_py,
        )
        for provider in version_providers:
            version = provider(source_path)
            if version:
                break
        else:
            raise DependencyParseError(
                f"Could not determine version for vendored library '{name}' at {source_path}"
            )

        self.dependencies.append(
            Dependency(name=name, version="==" + version, path=source_path.as_posix())
        )

    def _handle_pypi(self, rest: str) -> None:
        pkg_spec = rest.split(":", 1)[0].strip()
        m = re.match(r"^([A-Za-z0-9_\-]+)(.+)$", pkg_spec)
        if not m:
            raise DependencyParseError(f"Invalid pypi spec: '{pkg_spec}'")
        name, version = m.group(1), m.group(2).strip()
        if not version:
            raise DependencyParseError(f"Missing version in pypi spec: '{pkg_spec}'")
        self.dependencies.append(Dependency(name=name, version=version, path=None))

    def _version_from_metadata_files(self, path: Path) -> Optional[str]:
        def _extract_version(file_path: Path) -> Optional[str]:
            try:
                with file_path.open(encoding="utf-8") as file:
                    for line in file:
                        if line.startswith("Version:"):
                            return line.split(":", 1)[1].strip()
            except OSError:
                return None

        # METADATA/PKG-INFO may live in .egg-info/.dist-info
        # subdirs or at the package root, so we check both.
        candidates = []
        for entry in path.iterdir():
            if entry.is_dir():
                if entry.name.endswith(".egg-info"):
                    pkg_info = entry / "PKG-INFO"
                    if pkg_info.is_file():
                        candidates.append(pkg_info)
                elif entry.name.endswith(".dist-info"):
                    metadata = entry / "METADATA"
                    if metadata.is_file():
                        candidates.append(metadata)
            elif entry.is_file() and entry.name in {"METADATA", "PKG-INFO"}:
                candidates.append(entry)

        for file_path in candidates:
            version = _extract_version(file_path)
            if version:
                return version

        return None

    def _version_from_setup_py(self, path: Path) -> Optional[str]:
        setup_py = path / "setup.py"
        if not setup_py.is_file():
            return None

        try:
            output = subprocess.check_output(
                [sys.executable, "setup.py", "--version"],
                cwd=path,
                stderr=subprocess.DEVNULL,
            )
            version = output.decode().strip()
            return version
        except Exception:
            return None
