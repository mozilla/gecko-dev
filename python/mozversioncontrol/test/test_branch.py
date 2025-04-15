# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit

from mozversioncontrol import get_repository_object

STEPS = {
    "hg": [
        """
        hg bookmark test
        """,
        """
        echo "bar" > foo
        hg commit -m "second commit"
        """,
    ],
    "git": [
        """
        git checkout -b test
        """,
        """
        echo "bar" > foo
        git commit -a -m "second commit"
        """,
    ],
    "jj": [
        """
        jj bookmark set test
        """,
        """
        jj new -m "xyzzy" zzzzzzzz
        jj new -m "second commit" test
        echo "bar" > foo
        """,
    ],
}


def test_branch(repo):
    vcs = get_repository_object(repo.dir)
    if vcs.name in ("git", "jj"):
        assert vcs.branch == "master"
    else:
        assert vcs.branch is None

    repo.execute_next_step()
    assert vcs.branch == "test"

    repo.execute_next_step()
    assert vcs.branch == "test"

    vcs.update(vcs.head_ref)
    if repo.vcs == "jj":
        # jj "branches" do not auto-advance (in our JujutsuRepository
        # implementation, anyway), so this is the rev marked as the root of the
        # test branch.
        assert vcs.branch == "test"
    else:
        assert vcs.branch is None

    vcs.update("test")
    assert vcs.branch == "test"

    # for jj only, check that a topological branch with no bookmarks is not
    # considered a "branch":
    if repo.vcs == "jj":
        vcs.update("description('xyzzy')")
        assert vcs.branch is None


if __name__ == "__main__":
    mozunit.main()
