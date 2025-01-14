# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import unittest

import toml
from buildconfig import topsrcdir
from mozunit import main

from mozboot.util import MINIMUM_RUST_VERSION


class TestMSRV(unittest.TestCase):
    def test_msrv(self):
        """Ensure MSRV in mozboot and top-level Config.toml match."""

        cargo_toml = os.path.join(topsrcdir, "Cargo.toml")
        with open(cargo_toml, "r") as f:
            content = toml.load(f)
        workspace = content.get("workspace", {})
        rust_version = workspace.get("package", {}).get("rust-version")
        self.assertEqual(MINIMUM_RUST_VERSION, rust_version)


if __name__ == "__main__":
    main()
