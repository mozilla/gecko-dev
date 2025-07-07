# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import subprocess

from mach.decorators import Command, CommandArgument
from mozbuild.base import MozbuildObject
from mozbuild.nodeutil import find_node_executable

suites = [
    "aboutdebugging",
    "accessibility",
    "all",
    "application",
    "compatibility",
    "debugger",
    "framework",
    "netmonitor",
    "performance",
    "shared_components",
    "webconsole",
]


class DevToolsNodeTestRunner(MozbuildObject):
    """Run DevTools node tests."""

    def run_node_tests(self, suite=None, artifact=None):
        """Run the DevTools node test suites."""
        devtools_bin_dir = os.path.join(self.topsrcdir, "devtools", "client", "bin")
        test_runner_script = os.path.join(
            devtools_bin_dir, "devtools-node-test-runner.js"
        )

        if suite and suite not in suites:
            print(
                f"ERROR: Invalid suite '{suite}'. Valid suites are: {', '.join(suites)}"
            )
            return 1

        # Build the command to run
        node_binary, _ = find_node_executable()
        cmd = [node_binary, test_runner_script]

        # Add artifact argument if specified
        if artifact:
            cmd.append(f"--artifact={artifact}")

        # Add suite argument
        cmd.append(f"--suite={suite}")

        print(f"Running: {' '.join(cmd)}")
        print(f"Working directory: {devtools_bin_dir}")

        try:
            # Run the test runner from the devtools bin directory
            result = subprocess.run(cmd, cwd=devtools_bin_dir, check=False)
            return result.returncode
        except FileNotFoundError:
            print(
                "ERROR: Node.js not found. Please ensure Node.js is installed and in your PATH."
            )
            return 1
        except Exception as e:
            print(f"ERROR: Failed to run DevTools node tests: {e}")
            return 1


@Command(
    "devtools-node-test",
    category="testing",
    description="Run DevTools node tests",
    parser=argparse.ArgumentParser(),
)
@CommandArgument(
    "--suite",
    default="all",
    help=f"(optional) Test suite to run. Runs all suites when omitted. Available suites: {', '.join(suites)}",
)
@CommandArgument(
    "--artifact",
    help="Path to write test error artifacts as JSON. Useful for CI integration "
    "and error reporting.",
)
def run_devtools_node_test(command_context, suite=None, artifact=None, **kwargs):
    """Run DevTools node tests."""
    runner = DevToolsNodeTestRunner.from_environment(
        cwd=os.getcwd(), detect_virtualenv_mozinfo=False
    )

    return runner.run_node_tests(suite=suite, artifact=artifact)
