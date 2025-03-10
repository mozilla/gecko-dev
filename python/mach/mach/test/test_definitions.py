# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from pathlib import Path

from mozunit import main

from mach.test.conftest import TestBase


class TestConditions(TestBase):
    """Tests for definitions provided to the @Command decorator."""

    def _run(self, args):
        return self._run_mach(args, Path("definitions.py"))

    def test_help_message(self):
        """Test that commands that are hidden do not show up in help."""

        result, stdout, stderr = self._run(["help"])
        self.assertIn("cmd_default_visible", stdout)
        self.assertIn("cmd_not_hidden", stdout)
        self.assertNotIn("cmd_hidden", stdout)


if __name__ == "__main__":
    main()
