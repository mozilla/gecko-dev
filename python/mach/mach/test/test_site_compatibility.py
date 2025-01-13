# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import shutil
import subprocess
import sys
from pathlib import Path
from textwrap import dedent

import mozunit
from buildconfig import topsrcdir
from packaging.specifiers import SpecifierSet

from mach.requirements import MachEnvRequirements
from mach.site import PythonVirtualenv


def _resolve_command_site_names():
    site_names = []
    for child in (Path(topsrcdir) / "python" / "sites").iterdir():
        if not child.is_file():
            continue

        if child.suffix != ".txt":
            continue

        if child.name == "mach.txt":
            continue

        site_names.append(child.stem)
    return site_names


def _requirement_definition_to_pip_format(site_name, cache, is_mach_or_build_env):
    """Convert from parsed requirements object to pip-consumable format"""
    requirements_path = Path(topsrcdir) / "python" / "sites" / f"{site_name}.txt"
    requirements = MachEnvRequirements.from_requirements_definition(
        topsrcdir, False, not is_mach_or_build_env, requirements_path
    )

    lines = []
    for pypi in (
        requirements.pypi_requirements + requirements.pypi_optional_requirements
    ):
        lines.append(str(pypi.requirement))

    for vendored in requirements.vendored_requirements:
        lines.append(str(cache.package_for_vendor_dir(Path(vendored.path))))

    for pth in requirements.pth_requirements:
        path = Path(pth.path)

        if "third_party" not in (p.name for p in path.parents):
            continue

        for child in path.iterdir():
            if child.name.endswith(".dist-info"):
                raise Exception(
                    f'In {requirements_path}, the "pth:" pointing to "{path}" has a '
                    '".dist-info" file.\n'
                    'Perhaps it should change to start with "vendored:" instead of '
                    '"pth:".'
                )
            if child.name.endswith(".egg-info"):
                raise Exception(
                    f'In {requirements_path}, the "pth:" pointing to "{path}" has an '
                    '".egg-info" file.\n'
                    'Perhaps it should change to start with "vendored:" instead of '
                    '"pth:".'
                )

    return "\n".join(lines), requirements.requires_python


class PackageCache:
    def __init__(self, storage_dir: Path):
        self._cache = {}
        self._storage_dir = storage_dir

    def package_for_vendor_dir(self, vendor_path: Path):
        if vendor_path in self._cache:
            return self._cache[vendor_path]

        if not any(p for p in vendor_path.iterdir() if p.name.endswith(".dist-info")):
            # This vendored package is not a wheel. It may be a source package (with
            # a setup.py), or just some Python code that was manually copied into the
            # tree. If it's a source package, the setup.py file may be up a few levels
            # from the referenced Python module path.
            package_dir = vendor_path
            while True:
                if (package_dir / "setup.py").exists():
                    break
                elif package_dir.parent == package_dir:
                    raise Exception(
                        f'Package "{vendor_path}" is not a wheel and does not have a '
                        'setup.py file. Perhaps it should be "pth:" instead of '
                        '"vendored:"?'
                    )
                package_dir = package_dir.parent

            self._cache[vendor_path] = package_dir
            return package_dir

        # Pip requires that wheels have a version number in their name, even if
        # it ignores it. We should parse out the version and put it in here
        # so that failure debugging is easier, but that's non-trivial work.
        # So, this "0" satisfies pip's naming requirement while being relatively
        # obvious that it's a placeholder.
        output_path = self._storage_dir / f"{vendor_path.name}-0-py3-none-any"
        shutil.make_archive(str(output_path), "zip", vendor_path)

        whl_path = output_path.parent / (output_path.name + ".whl")
        (output_path.parent / (output_path.name + ".zip")).rename(whl_path)
        self._cache[vendor_path] = whl_path

        return whl_path


def test_sites_compatible(tmpdir: str):
    python_version_for_check = "3.8"
    assert sys.version.startswith(python_version_for_check), (
        f"This is a test for mach's minimum supported version of Python {python_version_for_check},"
        f"but executing Python is {sys.version}. If this failure occurs in automation, it may mean"
        f"the worker's Python was upgraded and/or mach's minimum supported Python version changed."
        f"This test may need to be updated."
    )

    command_site_names = _resolve_command_site_names()
    work_dir = Path(tmpdir)
    cache = PackageCache(work_dir)
    mach_requirements, mach_requires_python = _requirement_definition_to_pip_format(
        "mach", cache, True
    )

    # Create virtualenv to try to install all dependencies into.
    virtualenv = PythonVirtualenv(str(work_dir / "env"))
    subprocess.check_call(
        [
            sys.executable,
            "-m",
            "venv",
            "--without-pip",
            virtualenv.prefix,
        ]
    )
    platlib_dir = virtualenv.resolve_sysconfig_packages_path("platlib")
    third_party = Path(topsrcdir) / "third_party" / "python"
    with open(os.path.join(platlib_dir, "site.pth"), "w") as pthfile:
        pthfile.write(
            "\n".join(
                [
                    str(third_party / "pip"),
                    str(third_party / "wheel"),
                    str(third_party / "setuptools"),
                ]
            )
        )

    for name in command_site_names:
        print(f'Checking compatibility of "{name}" site')
        command_requirements, command_requires_python = (
            _requirement_definition_to_pip_format(name, cache, name == "build")
        )

        command_specifier = SpecifierSet(command_requires_python)

        if not command_specifier.contains(python_version_for_check):
            print(
                f"Skipping the '{name}' site as its requirements ({command_requires_python}) "
                f"do not include {python_version_for_check}."
            )
            continue

        with open(work_dir / "requirements.txt", "w") as requirements_txt:
            requirements_txt.write(mach_requirements)
            requirements_txt.write("\n")
            requirements_txt.write(command_requirements)

        # Attempt to install combined set of dependencies (global Mach + current
        # command)
        proc = subprocess.run(
            [
                virtualenv.python_path,
                "-m",
                "pip",
                "install",
                "-r",
                str(work_dir / "requirements.txt"),
            ],
            cwd=topsrcdir,
        )
        if proc.returncode != 0:
            print(
                dedent(
                    f"""
                Error: The '{name}' site contains dependencies that are not
                compatible with the 'mach' site. Check the following files for
                any conflicting packages mentioned in the prior error message:

                  python/sites/mach.txt
                  python/sites/{name}.txt
                        """
                )
            )
            assert False


if __name__ == "__main__":
    mozunit.main()
