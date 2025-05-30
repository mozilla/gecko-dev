# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

LINTER = "python-sites"


def test_lint_python_sites(lint, paths):
    results = lint(paths())
    assert len(results) == 3

    for r in results:
        assert "bad/test.txt" in r.relpath
        assert r.level == "error"
        assert r.lineno == 1

    expected_messages = [
        "After 'requires-python:', entries must be in alphabetical order.",
        "First line must start with 'requires-python:'.",
        "Specification of 'vendored:third_party/python/blessed' is redundant; already in",
    ]
    actual_messages = [r.message for r in results]

    missing = [
        msg
        for msg in expected_messages
        if not any(msg in actual for actual in actual_messages)
    ]
    assert not missing, f"Missing expected message(s): {missing}"


if __name__ == "__main__":
    mozunit.main()
