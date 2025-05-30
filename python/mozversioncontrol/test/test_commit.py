# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
from datetime import datetime
from pathlib import Path

import mozunit

from mozversioncontrol import get_repository_object

STEPS = {
    "hg": [
        "",
        """
        echo "bar" >> bar
        echo "baz" > foo
        """,
    ],
    "git": [
        "",
        """
        echo "bar" >> bar
        echo "baz" > foo
        """,
    ],
    "jj": [
        """
        jj describe -m "Ignore file for testing"
        echo foo > .gitignore
        jj new
        """,
        """
        echo "bar" >> bar
        echo "baz" > foo
        """,
    ],
}


def test_commit(repo):
    vcs = get_repository_object(repo.dir)
    assert vcs.working_directory_clean()

    # Setup step for jj to allow untracked changes.
    repo.execute_next_step()

    # Modify both foo and bar
    repo.execute_next_step()
    if repo.vcs != "jj":
        # jj never has a dirty working directory.
        assert not vcs.working_directory_clean()

    date_string = "2017-07-14 02:40:00 +0000"

    # Commit just bar
    vcs.commit(
        message="Modify bar\n\nbut not baz",
        author="Testing McTesterson <test@example.org>",
        date=date_string,
        paths=["bar"],
    )

    original_date = datetime.strptime(date_string, "%Y-%m-%d %H:%M:%S %z")
    date_from_vcs = vcs.get_last_modified_time_for_file(Path("bar"))

    assert original_date == date_from_vcs

    # We only committed bar, so foo is still keeping the working dir dirty. jj
    # always treats the working directory as clean, because the top commit holds
    # any changes in it.
    if repo.vcs == "jj":
        assert vcs.working_directory_clean()
    else:
        assert not vcs.working_directory_clean()

    if repo.vcs == "git":
        log_cmd = ["log", "-1", "--format=%an,%ae,%aD,%B"]
        patch_cmd = ["log", "-1", "-p"]
    elif repo.vcs == "hg":
        log_cmd = [
            "log",
            "-l",
            "1",
            "-T",
            "{person(author)},{email(author)},{date|rfc822date},{desc}",
        ]
        patch_cmd = ["log", "-l", "1", "-p"]
    elif repo.vcs == "jj":
        log_cmd = [
            "log",
            "-n1",
            "--no-graph",
            "-r@-",
            "-T",
            'separate(",", author.name(), author.email(), commit_timestamp(self).format("%a, %d %b %Y %H:%M:%S %z"), description)',
        ]
        patch_cmd = ["show", "@-", "--git"]

    # Verify commit metadata (we rstrip to normalize trivial differences)
    log = vcs._run(*log_cmd).rstrip()
    assert log == (
        "Testing McTesterson,test@example.org,Fri, 14 "
        "Jul 2017 02:40:00 +0000,Modify bar\n\nbut not baz"
    )

    # Verify only the intended file was added to the commit
    patch = vcs._run(*patch_cmd)

    def find_diff_marker(patch: str, filename: str):
        patterns = [
            rf"^diff --git a/{re.escape(filename)} b/{re.escape(filename)}$",
            rf"^Modified regular file {re.escape(filename)}:$",
        ]

        matches = [
            line
            for line in patch.splitlines()
            if any(re.fullmatch(p, line) for p in patterns)
        ]

        assert matches, f"No diff marker found for '{filename}'"
        assert (
            len(matches) == 1
        ), f"More than one diff marker for '{filename}': {matches}"

        return matches[0]

    marker = find_diff_marker(patch, "bar")

    assert marker in [
        "diff --git a/bar b/bar",
        "Modified regular file bar:",
    ]


if __name__ == "__main__":
    mozunit.main()
