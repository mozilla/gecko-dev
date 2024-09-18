# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import json
import os
import pathlib
import platform
import shutil
import signal
import sys
import time

import mozprocess

from mozperftest.layers import Layer
from mozperftest.utils import ON_TRY, archive_folder, install_package, temp_dir


class UnknownScriptError(Exception):
    """Triggered when an unknown script type is encountered."""

    pass


class ShellScriptData:
    def open_data(self, data):
        return {
            "name": "shellscript",
            "subtest": data["name"],
            "data": [
                {"file": "custom", "value": value, "xaxis": xaxis}
                for xaxis, value in enumerate(data["values"])
            ],
            "shouldAlert": data.get("shouldAlert", True),
            "unit": data.get("unit", "ms"),
            "lowerIsBetter": data.get("lowerIsBetter", True),
        }

    def transform(self, data):
        return data

    merge = transform


class ShellScriptRunner(Layer):
    """
    This is a layer that can be used to run custom shell scripts. They
    are expected to produce a log message prefixed with `perfMetrics` and
    contain the data from the test.
    """

    name = "shell-script"
    activated = True
    arguments = {
        "output-timeout": {
            "default": 120,
            "help": "Output timeout for the script, or how long to wait for a log message.",
        },
        "process-timeout": {
            "default": 600,
            "help": "Process timeout for the script, or the max run time.",
        },
    }

    def __init__(self, env, mach_cmd):
        super(ShellScriptRunner, self).__init__(env, mach_cmd)
        self.metrics = []
        self.timed_out = False
        self.output_timed_out = False

    def setup(self):
        # Install opencv dependency
        deps = ["opencv-python==4.10.0.84"]
        for dep in deps:
            install_package(self.mach_cmd.virtualenv_manager, dep)

    def kill(self, proc):
        if "win" in platform.system().lower():
            proc.send_signal(signal.CTRL_BREAK_EVENT)
        else:
            os.killpg(proc.pid, signal.SIGKILL)
        proc.wait()

    def parse_metrics(self):
        parsed_metrics = []
        for metrics in self.metrics:
            prepared_metrics = (
                metrics.replace("perfMetrics:", "")
                .replace("{{", "{")
                .replace("}}", "}")
                .strip()
            )

            metrics_json = json.loads(prepared_metrics)
            if isinstance(metrics_json, list):
                parsed_metrics.extend(metrics_json)
            else:
                parsed_metrics.append(metrics_json)

        return parsed_metrics

    def line_handler_wrapper(self):
        """This function is used to gather the perfMetrics logs."""

        def _line_handler(proc, line):
            # NOTE: this hack is to workaround encoding issues on windows
            # a newer version of browsertime adds a `σ` character to output
            line = line.replace(b"\xcf\x83", b"")

            line = line.decode("utf-8")
            if "perfMetrics" in line:
                self.metrics.append(line)

            # Bug 1900056 - Use a different logger in mozperftest because the current
            # one can't handle messages with curly braces or JSONs in them
            print(line.strip())

        return _line_handler

    def run(self, metadata):
        test = metadata.script

        cmd = []
        if test["filename"].endswith(".sh"):
            cmd = ["bash", test["filename"]]
        else:
            raise UnknownScriptError(
                "Only `.sh` (bash) scripts are currently implemented."
            )

        def timeout_handler(proc):
            self.timed_out = True
            self.error("Process timed out")
            self.kill(proc)

        def output_timeout_handler(proc):
            self.output_timed_out = True
            self.error("Process output timed out")
            self.kill(proc)

        with temp_dir() as testing_dir:
            os.environ["APP"] = self.get_arg("app")
            os.environ["TESTING_DIR"] = testing_dir
            os.environ["BROWSER_BINARY"] = metadata.binary
            os.environ["PYTHON_PATH_SHELL_SCRIPT"] = sys.executable

            # Setup a python packages path for python scripts. This is required since
            # the sys.path modifications made by mozperftest don't get propagated
            # to the scripts. Furthermore, PYTHONPATH can't be used here since it
            # gets overrided by something like a ._pth file somewhere along the way.
            venv_site_lib = str(
                pathlib.Path(
                    self.mach_cmd.virtualenv_manager.bin_path, "..", "lib"
                ).resolve()
            )
            venv_site_packages = pathlib.Path(
                venv_site_lib,
                f"python{sys.version_info.major}.{sys.version_info.minor}",
                "site-packages",
            )
            if not venv_site_packages.exists():
                venv_site_packages = pathlib.Path(
                    venv_site_lib,
                    "site-packages",
                )
            os.environ["PYTHON_PACKAGES"] = str(venv_site_packages)

            mozprocess.run_and_wait(
                cmd,
                output_line_handler=self.line_handler_wrapper(),
                env=os.environ,
                timeout=self.get_arg("process-timeout"),
                timeout_handler=timeout_handler,
                output_timeout=self.get_arg("output-timeout"),
                output_timeout_handler=output_timeout_handler,
                text=False,
            )

            if self.get_arg("output"):
                if ON_TRY:
                    # In CI, make an archive of the files and upload those
                    archive_folder(
                        pathlib.Path(testing_dir),
                        pathlib.Path(self.get_arg("output")),
                        archive_name=test["name"],
                    )
                else:
                    output_dir = pathlib.Path(
                        self.get_arg("output"),
                        test["name"],
                        f"run-{str(time.time()).split('.')[0]}",
                    )
                    self.info(f"Copying testing directory to {output_dir}")
                    shutil.copytree(testing_dir, output_dir)

        metadata.add_result(
            {
                "name": test["name"],
                "framework": {"name": "mozperftest"},
                "transformer": "mozperftest.test.shellscript:ShellScriptData",
                "shouldAlert": True,
                "results": self.parse_metrics(),
            }
        )

        return metadata
