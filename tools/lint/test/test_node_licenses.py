# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import mozunit
from conftest import build

LINTER = "node-licenses"


def test_lint_unknown_license(lint, paths):
    results = lint(
        paths(os.path.join("bad", "package.json")),
        root=build.topsrcdir,
        skip_reinstall=True,
    )

    assert len(results) == 1
    assert results[0].level == "error"
    assert (
        "Included (sub-)dependency @mozilla/unknown-license@0.0.1 needs license UNKNOWN checking for acceptability"
        in results[0].message
    )
    assert "bad/package.json" in results[0].relpath
    assert results[0].lineno == 0


def test_lint_known_license(lint, paths):
    results = lint(
        paths(os.path.join("good", "package.json")),
        root=build.topsrcdir,
        skip_reinstall=True,
    )

    assert len(results) == 0


if __name__ == "__main__":
    mozunit.main()
