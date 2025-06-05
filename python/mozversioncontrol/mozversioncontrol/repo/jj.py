# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import re
import string
import subprocess
import sys
from contextlib import contextmanager
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

import mozpack.path as mozpath
from mozfile import which
from mozpack.files import FileListFinder
from packaging.version import Version

MINIMUM_SUPPORTED_JJ_VERSION = Version("0.28")
USING_JJ_DETECTED = 'Using JujutsuRepository because a ".jj/" directory was detected!'
USING_JJ_WARNING = """\

Warning: jj support is currently experimental, and may be disabled by setting the
environment variable MOZ_AVOID_JJ_VCS=1. (This warning may be suppressed by
setting MOZ_AVOID_JJ_VCS=0.)"""

from mozversioncontrol.errors import (
    CannotDeleteFromRootOfRepositoryException,
    MissingVCSExtension,
    MissingVCSInfo,
)
from mozversioncontrol.repo.base import Repository
from mozversioncontrol.repo.git import GitRepository


class JjVersionError(Exception):
    """Raised when the installed jj version is too old."""

    pass


class JujutsuRepository(Repository):
    """An implementation of `Repository` for JJ repositories using the git backend."""

    def __init__(self, path: Path, jj="jj", git="git"):
        super(JujutsuRepository, self).__init__(path, tool=jj)
        self._git = GitRepository(path, git=git)

        # Find git root. Newer jj has `jj git root`, but this should support
        # older versions for now.
        out = self._run("root")
        if not out:
            raise MissingVCSInfo("cannot find jj workspace root")

        try:
            jj_ws_root = Path(out.rstrip())
            jj_repo = jj_ws_root / ".jj" / "repo"
            if not jj_repo.is_dir():
                jj_repo = Path(jj_repo.read_text())
        except Exception:
            raise MissingVCSInfo("cannot find jj repo")

        try:
            git_target = jj_repo / "store" / "git_target"
            git_dir = git_target.parent / Path(git_target.read_text())
        except Exception:
            raise MissingVCSInfo("cannot find git dir")

        if not git_dir.is_dir():
            raise MissingVCSInfo("cannot find git dir")

        self._git._env["GIT_DIR"] = str(git_dir.resolve())

    def _run_read_only(self, *args, **kwargs):
        """_run_read_only() should be used instead of _run() for read-only jj commands.

        It will avoid locking the working copy and can prevent potential concurrency issues.
        """
        return super()._run("--ignore-working-copy", *args, **kwargs)

    def _snapshot(self):
        """_snapshot() can be used to update the repository after changing files in the working
        directory. Normally jj commands will do this automatically, but we often run jj commands
        using `_run_read_only` which passes `--ignore-working-copy` to jj.
        See bug 1962245 and bug 1962389.

        An alternative option would be to add an extra argument to methods such as
        `get_commits`.
        """
        self._run("log", "-n0")

    def _resolve_to_change(self, revset: str) -> Optional[str]:
        change_id = self._run_read_only(
            "log", "--no-graph", "-n1", "-r", revset, "-T", "change_id.short()"
        ).rstrip()
        return change_id if change_id != "" else None

    @property
    def name(self):
        return "jj"

    @property
    def head_ref(self):
        # This is not really a defined concept in jj. Map it to @, or rather the
        # persistent change id for the current @. Warning: this cannot be passed
        # directly to a git command, it must be converted to a commit id first
        # (eg via resolve_to_commit). This isn't done here because
        # callers should be aware when they're dropping down to git semantics.
        return self._resolve_to_change("@")

    def is_cinnabar_repo(self) -> bool:
        return self._git.is_cinnabar_repo()

    @property
    def base_ref(self):
        ref = self._resolve_to_change("latest(roots(::@ & mutable())-)")
        return ref if ref else self.head_ref

    def _resolve_to_commit(self, revset):
        commit = self._run_read_only(
            "log", "--no-graph", "-r", f"latest({revset})", "-T", "commit_id"
        ).rstrip()
        return commit

    def base_ref_as_hg(self):
        return self._git.base_ref_as_hg()

    def base_ref_as_commit(self):
        return self._resolve_to_commit(self.base_ref)

    @property
    def branch(self):
        # jj does not have an "active branch" concept. The lone caller will fall
        # back to self.head_ref.
        return None

    @property
    def has_git_cinnabar(self):
        return self._git.has_git_cinnabar

    def get_commit_time(self):
        return int(
            self._run_read_only(
                "log", "-n1", "--no-graph", "-T", 'committer.timestamp().format("%s")'
            ).strip()
        )

    def sparse_checkout_present(self):
        return self._run_read_only("sparse", "list").rstrip() != "."

    def get_user_email(self):
        email = self._run_read_only("config", "get", "user.email", return_codes=[0, 1])
        if not email:
            return None
        return email.strip()

    def get_changed_files(self, diff_filter="ADM", mode="(ignored)", rev="@"):
        assert all(f.lower() in self._valid_diff_filter for f in diff_filter)

        out = self._run_read_only(
            "log",
            "-r",
            rev,
            "--no-graph",
            "-T",
            'diff.files().map(|f| surround("", "\n", separate("\t", f.status(), f.source().path(), f.target().path()))).join("")',
        )
        changed = []
        for line in out.splitlines():
            op, source, target = line.split("\t")
            if op == "modified":
                if "M" in diff_filter:
                    changed.append(source)
            elif op == "added":
                if "A" in diff_filter:
                    changed.append(source)
            elif op == "removed":
                if "D" in diff_filter:
                    changed.append(source)
            elif op == "copied":
                if "A" in diff_filter:
                    changed.append(target)
            elif op == "renamed":
                if "A" in diff_filter:
                    changed.append(target)
                if "D" in diff_filter:
                    changed.append(source)
            else:
                raise Exception(f"unexpected jj file status '{op}'")

        return changed

    def diff_stream(self, rev=None, extensions=(), exclude_file=None, context=8):
        if rev is None:
            rev = "latest((@ ~ empty()) | @-)"
        rev = self._resolve_to_commit(rev)
        return self._git.diff_stream(
            rev=rev, extensions=extensions, exclude_file=exclude_file, context=context
        )

    def get_outgoing_files(self, diff_filter="ADM", upstream=None):
        assert all(f.lower() in self._valid_diff_filter for f in diff_filter)

        if upstream is None:
            upstream = self.base_ref

        lines = self._run_read_only(
            "diff",
            "--from",
            upstream,
            "--to",
            "@",
            "--summary",
        ).splitlines()

        outgoing = []
        for line in lines:
            op, file = line.split(" ", 1)
            if op.upper() in diff_filter:
                outgoing.append(mozpath.normsep(file))
        return outgoing

    def add_remove_files(self, *paths: Union[str, Path], force: bool = False):
        if not paths:
            return

        relative_paths = [self._repo_root_relative_path(p) for p in paths]
        self._run("file", "track", *relative_paths)

    def forget_add_remove_files(self, *paths: Union[str, Path]):
        if not paths:
            return

        relative_paths = [self._repo_root_relative_path(p) for p in paths]
        self._run("file", "untrack", *relative_paths)

    def get_tracked_files_finder(self, path=None):
        files = [mozpath.normsep(p) for p in self._run("file", "list").splitlines()]
        return FileListFinder(files)

    def get_ignored_files_finder(self):
        raise Exception("unimplemented")

    def working_directory_clean(self, untracked=False, ignored=False):
        # Working directory is in the top commit.
        return True

    def update(self, ref):
        self._run("new", ref)

    def edit(self, ref):
        self._run("edit", ref)

    def clean_directory(self, path: Union[str, Path]):
        if Path(self.path).samefile(path):
            raise CannotDeleteFromRootOfRepositoryException()

        self._run("restore", "-r", "@-", str(path))

    def commit(self, message, author=None, date=None, paths=None):
        run_kwargs = {}
        cmd = ["commit", "-m", message]
        if author:
            cmd += ["--author", author]
        if date:
            dt = datetime.strptime(date, "%Y-%m-%d %H:%M:%S %z")
            run_kwargs["env"] = {"JJ_TIMESTAMP": dt.isoformat()}
        if paths:
            cmd.extend(paths)
        self._run(*cmd, **run_kwargs)

    def push_to_try(
        self,
        message: str,
        changed_files: Dict[str, str] = {},
        allow_log_capture: bool = False,
    ):
        if not self.has_git_cinnabar:
            raise MissingVCSExtension("cinnabar")

        with self.try_commit(message, changed_files) as head:
            if "mach_tryserver" in self._git._run("remote"):
                self._run(
                    "git", "remote", "remove", "mach_tryserver", return_codes=[0, 1]
                )
            # `jj git remote add` would barf on the cinnabar syntax here.
            self._git._run(
                "remote", "add", "mach_tryserver", "hg::ssh://hg.mozilla.org/try"
            )
            self._run("git", "import")
            cmd = (
                str(self._tool),
                "git",
                "push",
                "--remote",
                "mach_tryserver",
                "--change",
                head,
                "--allow-new",
                "--allow-empty-description",
            )
            if allow_log_capture:
                self._push_to_try_with_log_capture(
                    cmd,
                    {
                        "stdout": subprocess.PIPE,
                        "stderr": subprocess.STDOUT,
                        "cwd": self.path,
                        "universal_newlines": True,
                        "bufsize": 1,
                    },
                )
            else:
                subprocess.check_call(cmd, cwd=self.path)
        self._run("git", "remote", "remove", "mach_tryserver", return_codes=[0, 1])

    def set_config(self, name, value):
        self._run_read_only("config", name, value)

    def get_commits(
        self,
        head: Optional[str] = "@",
        limit: Optional[int] = None,
        follow: Optional[List[str]] = None,
    ) -> List[str]:
        """Return a list of commit SHAs for nodes on the current branch, in order that they should be applied."""
        # Note: lando gets grumpy if you try to push empty commits.
        cmd = [
            "log",
            "--no-graph",
            "-r",
            f"(::{head} & mutable()) ~ empty()",
            "-T",
            'commit_id ++ "\n"',
        ]
        if limit is not None:
            cmd.append(f"-n{limit}")
        if follow is not None:
            cmd.extend(follow)

        return list(reversed(self._run_read_only(*cmd).splitlines()))

    def _looks_like_change_id(self, id):
        return len(id) > 0 and all(letter >= "k" and letter <= "z" for letter in id)

    def _looks_like_commit_id(self, id):
        return len(id) > 0 and all(letter in string.hexdigits for letter in id)

    def get_commit_patches(self, nodes: List[str]) -> List[bytes]:
        """Return the contents of the patch `node` in the git standard format."""
        # Warning: tests, at least, may call this with change ids rather than
        # commit ids.
        nodes = [
            id if self._looks_like_commit_id(id) else self._resolve_to_commit(id)
            for id in nodes
        ]
        return [
            self._git._run(
                "format-patch", node, "-1", "--always", "--stdout", encoding=None
            )
            for node in nodes
        ]

    @contextmanager
    def try_commit(
        self, commit_message: str, changed_files: Optional[Dict[str, str]] = None
    ):
        """Create a temporary try commit as a context manager.

        Create a new commit using `commit_message` as the commit message. The commit
        may be empty, for example when only including try syntax.

        `changed_files` may contain a dict of file paths and their contents,
        see `stage_changes`.
        """
        # Redundant with the snapshot from the next command, but the semantics
        # of this operation depend on a snapshot happening (and it will eat
        # working-copy changes if not!), so be extra explicit here in case it
        # becomes possible to default snapshotting off.
        self._run("debug", "snapshot")  # Force a snapshot.
        opid = self._run(
            "operation", "log", "-n1", "--no-graph", "-T", "id.short(16)"
        ).rstrip()
        try:
            self._run("new", "-m", commit_message, "latest((@ ~ empty()) | @-)")
            for path, content in (changed_files or {}).items():
                p = self.path / Path(path)
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(content)
            # Update the jj commit with the changes we just made.
            self._snapshot()
            yield self._resolve_to_change("@")
        finally:
            self._run("operation", "restore", opid)

    def get_last_modified_time_for_file(self, path: Path) -> datetime:
        """Return last modified in VCS time for the specified file."""
        date = self._run_read_only(
            "log",
            "--no-graph",
            "-n1",
            "-T",
            "committer.timestamp()",
            '"%s"' % str(path).replace("\\", "\\\\"),
        ).rstrip()
        return datetime.strptime(date, "%Y-%m-%d %H:%M:%S.%f %z")

    def config_key_list_value_missing(self, key: str):
        output = self._run_read_only("config", "list", key, stderr=subprocess.STDOUT)
        warning_prefix = "Warning: No matching config key"
        if output.startswith(warning_prefix):
            return True

        if output.startswith(key):
            return False

        raise ValueError(f"Unexpected output: {output}")

    def set_config_key_value(self, key: str, value: Any):
        value_str = json.dumps(value)
        print(f'Set jj config: "{key} = {value_str}"')
        self._run("config", "set", "--repo", key, value_str)

    def configure(self, state_dir: Path, update_only: bool = False):
        """Run the Jujutsu configuration steps."""
        print(USING_JJ_WARNING, file=sys.stderr)
        print(
            "\nOur jj support currently relies on Git; checks will run for both jj and Git.\n"
        )

        self._git.configure(state_dir, update_only)

        topsrcdir = Path(self.path)
        if not update_only:
            print("\nConfiguring jj...")

            version_str = self._run_read_only("--version")
            if match := re.search(r"(\d+\.\d+\.\d+)", version_str):
                jj_version = Version(match.group(1))
            else:
                raise Exception("Could not find jj version")

            if jj_version < MINIMUM_SUPPORTED_JJ_VERSION:
                raise JjVersionError(
                    f"Your version of jj ({jj_version}) is too old. "
                    f"Please upgrade to at least version '{MINIMUM_SUPPORTED_JJ_VERSION}' to ensure "
                    "full compatibility and performance."
                )

            print(f"Detected jj version `{jj_version}`, which is sufficiently modern.")

            # Only set these values if they haven't been set yet so that we
            # don't overwrite existing user preferences.

            updated_author = False

            # Copy over the user.name and user.email if they've been set in git but not in jj
            username_key = "user.name"
            git_username = self._git.get_config_key_value(username_key)
            jj_username_missing = self.config_key_list_value_missing(username_key)
            if git_username and jj_username_missing:
                self.set_config_key_value(username_key, git_username)
                updated_author = True

            email_key = "user.email"
            git_email = self._git.get_config_key_value(email_key)
            jj_email_missing = self.config_key_list_value_missing(email_key)
            if git_email and jj_email_missing:
                self.set_config_key_value(email_key, git_email)
                updated_author = True

            if updated_author:
                self._run("describe", "--reset-author", "--no-edit")

            jj_revset_immutable_heads_key = 'revset-aliases."immutable_heads()"'
            if self.config_key_list_value_missing(jj_revset_immutable_heads_key):
                jj_revset_immutable_heads_value = (
                    "builtin_immutable_heads() | remote_bookmarks(glob:'*', 'origin')"
                )
                self.set_config_key_value(
                    jj_revset_immutable_heads_key, jj_revset_immutable_heads_value
                )

            # This enables `jj fix` which does `./mach lint --fix` on every commit in parallel
            jj_fix_command_key = "fix.tools.mozlint.command"
            if self.config_key_list_value_missing(jj_fix_command_key):
                jj_fix_command_value = [
                    f"{topsrcdir.as_posix()}/tools/lint/pipelint",
                    "$path",
                ]
                if sys.platform.startswith("win"):
                    # On Windows pipelint must be invoked via Python explicitly
                    jj_fix_command_value.insert(0, "python3")
                self.set_config_key_value(jj_fix_command_key, jj_fix_command_value)

            jj_fix_patterns_key = "fix.tools.mozlint.patterns"
            if self.config_key_list_value_missing(jj_fix_patterns_key):
                jj_fix_patterns_value = ["glob:**/*"]
                self.set_config_key_value(jj_fix_patterns_key, jj_fix_patterns_value)

            # This enables watchman if it's installed.
            if which("watchman"):
                jj_watchman_key = "core.fsmonitor"
                if self.config_key_list_value_missing(jj_watchman_key):
                    jj_watchman_value = "watchman"
                    self.set_config_key_value(jj_watchman_key, jj_watchman_value)

                jj_watchman_snapshot_key = "core.watchman.register-snapshot-trigger"
                if self.config_key_list_value_missing(jj_watchman_snapshot_key):
                    jj_watchman_snapshot_value = False
                    self.set_config_key_value(
                        jj_watchman_snapshot_key, jj_watchman_snapshot_value
                    )

                print("Checking if watchman is enabled...")
                output = self._run_read_only("debug", "watchman", "status")

                pattern = re.compile(
                    r"^Background snapshotting is (disabled|currently inactive)"
                )

                for line in output.splitlines():
                    # Filter out any 'Background snapshotting is' disabled/inactive messages.
                    # Snapshotting is disabled on purpose, and these lines could mislead users
                    # into thinking watchman isn't functioning. We only need to verify watchman status
                    # without potentially misleading users into enabling snapshotting and causing issues.
                    if not pattern.match(line):
                        print(line)
            else:
                print(
                    "Watchman could not be found on the PATH. It is recommended to "
                    "install watchman to improve performance for jj operations"
                )
