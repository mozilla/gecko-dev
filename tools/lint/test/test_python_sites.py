# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

LINTER = "python-sites"


def test_lint_python_sites(lint, paths):
    results = lint(paths())
    assert len(results) == 3

    assert "bad/test.txt" in results[0].relpath
    assert results[0].level == "error"
    assert "First line must start with 'requires-python:'." in results[0].message
    assert results[0].lineno == 1

    assert "bad/test.txt" in results[1].relpath
    assert results[1].level == "error"
    assert (
        "Specification of 'vendored:third_party/python/blessed' is redundant; already in"
        in results[1].message
    )
    assert results[1].lineno == 1

    assert "bad/test.txt" in results[2].relpath
    assert results[2].level == "error"
    assert (
        "After 'requires-python:', entries must be in alphabetical order."
        in results[2].message
    )
    assert results[2].lineno == 1


if __name__ == "__main__":
    mozunit.main()
