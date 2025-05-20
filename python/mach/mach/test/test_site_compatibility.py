# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import subprocess
import sys

import mozunit
from buildconfig import topsrcdir


def test_sites_compatible(tmpdir: str):
    result = subprocess.run(
        [sys.executable, "mach", "generate-python-lockfiles"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=topsrcdir,
        text=True,
    )

    # We pipe stderr to stdout and print here so that on error, the combined output
    # appears together in the test logs, making it much easier to read.
    print(result.stdout)

    assert result.returncode == 0


if __name__ == "__main__":
    mozunit.main()
