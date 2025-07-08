# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

import json
import os
import signal
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "eslint"))
from eslint import setup_helper
from mozbuild.nodeutil import find_node_executable
from mozlint import result
from mozlint.pathutils import expand_exclusions

here = os.path.abspath(os.path.dirname(__file__))
topsrcdir = os.path.join(here, "..", "..", "..")

NODE_LICENSES_ERROR_MESSAGE = """
An error occurred running license-checker. Please check the following error messages:

{}
""".strip()

NODE_LICENSES_NOT_FOUND_MESSAGE = """
Could not find license-checker!  We looked at the --binary option and at your
local node_modules path. Please install license checker and needed plugins with:

mach lint -l license-checker --setup

and try again.
""".strip()


def setup(root, **lintargs):
    setup_helper.set_project_root(root)

    if not setup_helper.check_node_executables_valid():
        return 1

    return setup_helper.eslint_maybe_setup()


def lint(paths, config, binary=None, skip_reinstall=False, **lintargs):
    setup_helper.set_project_root(lintargs["root"])

    paths = list(expand_exclusions(paths, config, lintargs["root"]))

    issues = []
    for path in paths:
        dirname = os.path.dirname(path)

        # Always ensure node_modules for the directory is up to date, so that we
        # are checking against the package-lock versions. Note that the checker
        # will not report if node_modules is missing.
        # We skip the setup in a few scenarios:
        # - For tests (skip_reinstall=True)
        # - In automation, because the node_modules are installed from the
        #   toolchains.
        # - When the package is the top-level package.json, because the top level
        #   node_modules is looked after by the setup, and we don't want to be
        #   removing it from underneath ourselves.
        if (
            not skip_reinstall
            and not os.environ.get("MOZ_AUTOMATION")
            and dirname != lintargs["root"]
        ):
            status = setup_helper.package_setup(
                dirname, os.path.basename(dirname), skip_logging=True
            )
            if status:
                issues.append(
                    result.from_config(
                        config,
                        **{
                            "path": path,
                            "message": "Unable to install node_modules for this package, try running 'npm ci' in the directory to debug",
                            "level": "error",
                        }
                    )
                )

        output = run_license_checker(binary, path, lintargs)
        if output == 1:
            return output

        def check_license(package, license):
            if not is_acceptable(config, package, license):
                res = {
                    "path": path,
                    "message": "Included (sub-)dependency "
                    + package
                    + " needs license "
                    + license
                    + " checking for acceptability",
                    "level": "error",
                }
                issues.append(result.from_config(config, **res))

        for package in output:
            if type(output[package]["licenses"]) is list:
                for license in output[package]["licenses"]:
                    check_license(package, license)
            else:
                check_license(package, output[package]["licenses"])

    return {"results": issues, "fixed": 0}


def run_license_checker(binary, path, lintargs):
    log = lintargs["log"]
    module_path = setup_helper.get_project_root()

    if not binary:
        binary, _ = find_node_executable()

    if not binary:
        log.error(NODE_LICENSES_NOT_FOUND_MESSAGE)
        return 1

    extra_args = lintargs.get("extra_args") or []

    cmd_args = [
        binary,
        os.path.join(
            module_path,
            "node_modules",
            "license-checker-rseidelsohn",
            "bin",
            "license-checker-rseidelsohn",
        ),
        "--json",
        "--start",
        os.path.dirname(path),
        "--excludePrivatePackages",
    ] + extra_args

    log.debug("license-checker command: {}".format(" ".join(cmd_args)))

    shell = False
    if is_windows():
        # The license checker binary needs to be run from a shell with msys
        shell = True

    orig = signal.signal(signal.SIGINT, signal.SIG_IGN)

    proc = subprocess.run(
        cmd_args, shell=shell, capture_output=True, text=True, check=False
    )

    signal.signal(signal.SIGINT, orig)

    errors = proc.stderr

    if proc.returncode != 0 or (errors and "An error has occurred:" in errors):
        print("license-checker reported an issue.")
        print(errors)
        return 1

    output = proc.stdout

    if not output:
        print("license-checker reported no output.")
        return 1

    try:
        jsonresult = json.loads(output)
    except ValueError:
        print(NODE_LICENSES_ERROR_MESSAGE.format(output))
        return 1

    return jsonresult


def is_acceptable(config, package, licenses):
    if licenses in config["accepted-test-licenses"]:
        return True

    package_name = package.split("@")[0]

    if package_name in config["known-packages"]:
        if config["known-packages"][package_name] == licenses:
            return True

    return False


def is_windows():
    return (
        os.environ.get("MSYSTEM") in ("MINGW32", "MINGW64")
        or "MOZILLABUILD" in os.environ
    )
