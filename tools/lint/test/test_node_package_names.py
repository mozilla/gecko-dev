# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

LINTER = "node-package-names"


def test_lint_with_global_exclude(lint, paths):
    results = lint(paths())
    assert len(results) == 1
    assert results[0].level == "error"
    assert (
        "package.json should not supply a name unless it is published."
        in results[0].message
    )
    assert "bad/package.json" in results[0].relpath
    assert results[0].lineno == 0


if __name__ == "__main__":
    mozunit.main()
