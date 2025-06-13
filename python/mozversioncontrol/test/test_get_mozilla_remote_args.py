# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from unittest import mock

import mozunit
import pytest

from mozversioncontrol import get_repository_object

STEPS = {
    "hg": [],
    "git": [
        "git remote add blah https://example.com/blah",
        """
        git remote add unified hg::https://hg.mozilla.org/mozilla-unified
        git remote add central hg::https://hg.mozilla.org/central
        git remote add try hg::https://hg.mozilla.org/try
        git remote add firefox https://github.com/mozilla-firefox/firefox
        """,
    ],
    "jj": [],
}


@pytest.mark.parametrize(
    "is_cinnabar,expected_remotes",
    (
        (
            True,
            [
                "--remotes=central",
                "--remotes=firefox",
                "--remotes=unified",
            ],
        ),
        (False, ["--remotes=firefox"]),
    ),
)
def test_get_mozilla_remote_args(is_cinnabar, expected_remotes, repo):
    # Test is only relevant for Git.
    if not repo.vcs == "git":
        return

    repo.execute_next_step()

    vcs = get_repository_object(repo.dir)
    vcs.is_cinnabar_repo = mock.MagicMock(name="is_cinnabar_repo")
    vcs.is_cinnabar_repo.return_value = is_cinnabar

    remotes = vcs.get_mozilla_remote_args()

    assert remotes == [
        "--remotes"
    ], "Default `--remotes` passed without finding official remote."

    repo.execute_next_step()

    remotes = sorted(vcs.get_mozilla_remote_args())

    assert (
        remotes == expected_remotes
    ), "Multiple non-try remote arguments should be found."


if __name__ == "__main__":
    mozunit.main()
