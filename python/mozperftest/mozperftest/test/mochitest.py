# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
from contextlib import redirect_stdout
from pathlib import Path

from mozperftest.layers import Layer
from mozperftest.test.functionaltestrunner import FunctionalTestRunner
from mozperftest.utils import (
    METRICS_MATCHER,
    ON_TRY,
    LogProcessor,
    NoPerfMetricsError,
    install_requirements_file,
)


class MissingMochitestInformation(Exception):
    """Raised when information needed to run a mochitest is missing."""

    pass


class MochitestTestFailure(Exception):
    """Raised when a mochitest test returns a non-zero exit code."""

    pass


class MochitestData:
    def open_data(self, data):
        return {
            "name": "mochitest",
            "subtest": data["name"],
            "data": [
                {"file": "mochitest", "value": value, "xaxis": xaxis}
                for xaxis, value in enumerate(data["values"])
            ],
            "value": data.get("value", None),
            "unit": data.get("unit", None),
            "shouldAlert": data.get("shouldAlert", None),
            "lowerIsBetter": data.get("lowerIsBetter", None),
        }

    def transform(self, data):
        return data

    merge = transform


class Mochitest(Layer):
    """Runs a mochitest test through `mach test` locally, and directly with mochitest in CI."""

    name = "mochitest"
    activated = True

    arguments = {
        "binary": {
            "type": str,
            "default": None,
            "help": ("Path to the browser."),
        },
        "cycles": {
            "type": int,
            "default": 1,
            "help": ("Number of cycles/iterations to do for the test."),
        },
        "manifest": {
            "type": str,
            "default": None,
            "help": (
                "Path to the manifest that contains the test (only required in CI)."
            ),
        },
        "manifest-flavor": {
            "type": str,
            "default": None,
            "help": "Mochitest flavor of the test to run (only required in CI).",
        },
        "extra-args": {
            "nargs": "*",
            "type": str,
            "default": [],
            "help": (
                "Additional arguments to pass to mochitest. Expected in a format such as: "
                "--mochitest-extra-args headless profile-path=/path/to/profile"
            ),
        },
    }

    def __init__(self, env, mach_cmd):
        super(Mochitest, self).__init__(env, mach_cmd)
        self.topsrcdir = mach_cmd.topsrcdir
        self._mach_context = mach_cmd._mach_context
        self.python_path = mach_cmd.virtualenv_manager.python_path
        self.topobjdir = mach_cmd.topobjdir
        self.distdir = mach_cmd.distdir
        self.bindir = mach_cmd.bindir
        self.statedir = mach_cmd.statedir
        self.metrics = []
        self.topsrcdir = mach_cmd.topsrcdir

    def setup(self):
        if ON_TRY:
            # Install marionette requirements
            install_requirements_file(
                self.mach_cmd.virtualenv_manager,
                str(
                    Path(
                        os.getenv("MOZ_FETCHES_DIR"),
                        "config",
                        "marionette_requirements.txt",
                    )
                ),
            )

    def _enable_gecko_profiling(self):
        """Setup gecko profiling if requested."""
        gecko_profile_args = []

        gecko_profile_features = os.getenv(
            "MOZ_PROFILER_STARTUP_FEATURES", "js,stackwalk,cpu,screenshots,memory"
        )
        gecko_profile_threads = os.getenv(
            "MOZ_PROFILER_STARTUP_FILTERS", "GeckoMain,Compositor,Renderer"
        )
        gecko_profile_entries = os.getenv("MOZ_PROFILER_STARTUP_ENTRIES", "65536000")
        gecko_profile_interval = os.getenv("MOZ_PROFILER_STARTUP_INTERVAL", None)

        if self.get_arg("gecko-profile") or os.getenv("MOZ_PROFILER_STARTUP") == "1":
            gecko_profile_args.append("--profiler")
            gecko_profile_args.extend(
                [
                    f"--setenv=MOZ_PROFILER_STARTUP_FEATURES={gecko_profile_features}",
                    f"--setenv=MOZ_PROFILER_STARTUP_FILTERS={gecko_profile_threads}",
                    f"--setenv=MOZ_PROFILER_STARTUP_ENTRIES={gecko_profile_entries}",
                ]
            )
            if gecko_profile_interval:
                gecko_profile_args.append(
                    f"--setenv=MOZ_PROFILER_STARTUP_INTERVAL={gecko_profile_interval}"
                )
            if ON_TRY:
                gecko_profile_args.append("--profiler-save-only")

                output_dir_path = str(Path(self.get_arg("output")).resolve())
                gecko_profile_args.append(f"--setenv=MOZ_UPLOAD_DIR={output_dir_path}")

        return gecko_profile_args

    def _parse_extra_args(self):
        """Sets up the extra-args from the user for passing to mochitest."""
        parsed_extra_args = []
        for arg in self.get_arg("extra-args"):
            parsed_extra_args.append(f"--{arg}")
        return parsed_extra_args

    def _get_mochitest_args(self):
        """Handles setup for all mochitest-specific arguments."""
        mochitest_args = []
        mochitest_args.extend(self._enable_gecko_profiling())
        mochitest_args.extend(self._parse_extra_args())
        return mochitest_args

    def remote_run(self, test, metadata):
        """Run tests in CI."""
        import runtests
        from manifestparser import TestManifest
        from mochitest_options import MochitestArgumentParser

        manifest_flavor = self.get_arg("manifest-flavor")
        manifest_name = self.get_arg("manifest")
        if not manifest_name:
            raise MissingMochitestInformation(
                "Name of manifest that contains test needs to be"
                "specified (e.g. mochitest-common.ini)"
            )
        if not manifest_flavor:
            raise MissingMochitestInformation(
                "Mochitest flavor needs to be provided"
                "(e.g. plain, browser-chrome, ...)"
            )

        manifest_path = Path(test.parent, manifest_name)
        manifest = TestManifest([str(manifest_path)], strict=False)
        manifest.active_tests(paths=[str(test)])

        # Use the mochitest argument parser to parse the extra argument
        # options, and produce an `args` object that has all the defaults
        parser = MochitestArgumentParser()
        args = parser.parse_args(self._get_mochitest_args())

        # Bug 1858155 - Attempting to only use one test_path triggers a failure
        # during test execution
        args.test_paths = [str(test.name), str(test.name)]
        args.keep_open = False
        args.runByManifest = True
        args.manifestFile = manifest
        args.topobjdir = self.topobjdir
        args.topsrcdir = self.topsrcdir
        args.flavor = manifest_flavor
        args.app = self.get_arg("mochitest_binary")

        fetch_dir = os.getenv("MOZ_FETCHES_DIR")
        args.utilityPath = str(Path(fetch_dir, "bin"))
        args.extraProfileFiles.append(str(Path(fetch_dir, "bin", "plugins")))
        args.testingModulesDir = str(Path(fetch_dir, "modules"))
        args.symbolsPath = str(Path(fetch_dir, "crashreporter-symbols"))
        args.certPath = str(Path(fetch_dir, "certs"))

        log_processor = LogProcessor(METRICS_MATCHER)
        with redirect_stdout(log_processor):
            result = runtests.run_test_harness(parser, args)

        return result, log_processor

    def run(self, metadata):
        test = Path(metadata.script["filename"])

        results = []
        cycles = self.get_arg("cycles", 1)
        for cycle in range(1, cycles + 1):
            if ON_TRY:
                status, log_processor = self.remote_run(test, metadata)
            else:
                status, log_processor = FunctionalTestRunner.test(
                    self.mach_cmd,
                    [str(test)],
                    self._get_mochitest_args() + ["--keep-open=False"],
                )

            if status is not None and status != 0:
                raise MochitestTestFailure("Test failed to run")

            # Parse metrics found
            for metrics_line in log_processor.match:
                self.metrics.append(json.loads(metrics_line.split("|")[-1].strip()))

        for m in self.metrics:
            # Expecting results like {"metric-name": value, "metric-name2": value, ...}
            if isinstance(m, dict):
                for key, val in m.items():
                    for r in results:
                        if r["name"] == key:
                            r["values"].append(val)
                            break
                    else:
                        results.append({"name": key, "values": [val]})
            # Expecting results like [
            #     {"name": "metric-name", "values": [value1, value2, ...], ...},
            #     {"name": "metric-name2", "values": [value1, value2, ...], ...},
            # ]
            else:
                for metric in m:
                    for r in results:
                        if r["name"] == metric["name"]:
                            r["values"].extend(metric["values"])
                            break
                    else:
                        results.append(metric)

        if len(results) == 0:
            raise NoPerfMetricsError("mochitest")

        metadata.add_result(
            {
                "name": test.name,
                "framework": {"name": "mozperftest"},
                "transformer": "mozperftest.test.mochitest:MochitestData",
                "results": results,
            }
        )

        return metadata
