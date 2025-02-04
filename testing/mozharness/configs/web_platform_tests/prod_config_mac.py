# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys

config = {
    "options": [
        "--prefs-root=%(test_path)s/prefs",
        "--config=%(test_path)s/wptrunner.ini",
        "--ca-cert-path=%(test_path)s/tests/tools/certs/cacert.pem",
        "--host-key-path=%(test_path)s/tests/tools/certs/web-platform.test.key",
        "--host-cert-path=%(test_path)s/tests/tools/certs/web-platform.test.pem",
        "--certutil-binary=%(test_install_path)s/bin/certutil",
    ],
    "geckodriver": os.path.join("%(abs_fetches_dir)s", "geckodriver"),
    "per_test_category": "web-platform",
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
