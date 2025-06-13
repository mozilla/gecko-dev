# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Optional

import tomlkit
from mozfile import which

from mozbuild.base import MozbuildObject
from mozbuild.lockfiles.site_dependency_extractor import SiteDependencyExtractor


class MissingMachSiteFileError(Exception):
    """Raised when the required mach.txt site file is missing."""

    pass


class MissingUVError(Exception):
    """Raised when the required 'uv' executable is not on PATH."""

    pass


class GeneratePythonLockfiles(MozbuildObject):

    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, virtualenv_name="vendor", **kwargs)

        self.keep_lockfiles = False
        self.output_dir = None
        self.sites_dir = Path(self.topsrcdir) / "python" / "sites"
        self.command_site_names_to_lock = {}
        self.mach_dependencies = []
        self.return_code = 0

    def setup(self, keep_lockfiles: bool, sites: Optional[list[str]] = None) -> None:
        self.keep_lockfiles = keep_lockfiles

        all_sites = {
            p.stem
            for p in self.sites_dir.iterdir()
            if p.is_file() and p.suffix == ".txt"
        }

        if sites:
            sites = set(sites)
        else:
            sites = all_sites

        # We will always create a lockfile for the `mach` site, so let's
        # make this set only for the command `sites`.
        self.command_site_names_to_lock = sites
        self.command_site_names_to_lock.discard("mach")

        # It should be impossible to get here without a `mach.txt`
        # file, but we'll do a sanity check anyway.
        mach_file = self.sites_dir / "mach.txt"
        if not mach_file.exists():
            raise MissingMachSiteFileError(
                f"Required site file 'mach.txt' not found in {self.sites_dir}"
            )

        # UV should be installed with the virtualenv, but we'll verify anyway.
        if which("uv") is None:
            raise MissingUVError("'uv' executable not found in PATH")

    def cleanup(self):
        if self.keep_lockfiles:
            print(f"\nGenerated lockfiles are retained in {self.output_dir}")
        else:
            shutil.rmtree(self.output_dir)

    def create_pyproject_toml_for_site(self, site_name: str):
        site_dir = self.output_dir / site_name
        site_dir.mkdir()

        subprocess.check_call(
            [
                "uv",
                "init",
                f"--name={site_name}-lock",
                "--bare",
                f"--description=Used to generate the lockfile for the {site_name} site.",
                "--no-progress",
            ],
            cwd=site_dir,
        )

        return site_dir / "pyproject.toml"

    def lock_site(self, site_name: str):
        print(f"\nPreparing to lock the `{site_name}` site")

        extractor = SiteDependencyExtractor(
            site_name=site_name, sites_dir=self.sites_dir, topsrcdir=self.topsrcdir
        )
        requires_python, dependencies = extractor.parse()

        # We always lock the mach site first and save its dependencies.
        # For all other sites, append those saved dependencies since they inherit from mach.
        if site_name == "mach":
            self.mach_dependencies = dependencies
        else:
            dependencies += self.mach_dependencies

        pyproject_toml_path = self.create_pyproject_toml_for_site(site_name=site_name)
        toml_doc = tomlkit.parse(pyproject_toml_path.read_text(encoding="utf-8"))
        project = toml_doc.setdefault("project", tomlkit.table())

        project["requires-python"] = requires_python

        deps = tomlkit.array()
        deps.multiline(True)
        deps.indent(4)
        for dep in dependencies:
            deps.append(dep.name + dep.version)

        project["dependencies"] = deps

        pyproject_toml_path.write_text(tomlkit.dumps(toml_doc), encoding="utf-8")

        print(f"Attempting to create a lockfile for the `{site_name}` site")
        result = subprocess.run(
            ["uv", "lock", "--no-progress"], cwd=pyproject_toml_path.parent, check=False
        )

        if result.returncode:
            print(f"Failed to create lockfile for `{site_name}` site...")
            if not self.return_code:
                self.return_code = result.returncode
        else:
            print(f"Successfully created lockfile for {site_name}")

    def run(self, keep_lockfiles: bool, sites: Optional[list[str]] = None) -> int:
        try:
            self.output_dir = Path(tempfile.mkdtemp(prefix="mach_python_lockfiles_"))
            self.setup(keep_lockfiles, sites)
            self.lock_site(site_name="mach")

            for site_name in self.command_site_names_to_lock:
                self.lock_site(site_name=site_name)

        finally:
            self.cleanup()

        return self.return_code
