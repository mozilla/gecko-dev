# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Configuration over-rides for prod_config.py, for osx
import os
import sys

#####
config = {
    "tooltool_cache": "/builds/tooltool_cache",
    "run_cmd_checks_enabled": True,
    "preflight_run_cmd_suites": [
        {
            "name": "verify refresh rate",
            "cmd": [
                sys.executable,
                os.path.join(
                    os.getcwd(),
                    "mozharness",
                    "external_tools",
                    "macosx_resolution_refreshrate.py",
                ),
                "--check=refresh-rate",
            ],
            "architectures": ["64bit"],
            "halt_on_failure": False,
            "enabled": True,
        },
        {
            "name": "verify screen resolution",
            "cmd": [
                sys.executable,
                os.path.join(
                    os.getcwd(),
                    "mozharness",
                    "external_tools",
                    "macosx_resolution_refreshrate.py",
                ),
                "--check=resolution",
            ],
            "architectures": ["64bit"],
            "halt_on_failure": False,
            "enabled": True,
        },
    ],
}
