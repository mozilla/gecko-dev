# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from pathlib import Path
from typing import Dict, Union

from mozpack.files import FileListFinder

from mozversioncontrol.errors import MissingVCSTool
from mozversioncontrol.repo.base import Repository


class SrcRepository(Repository):
    """An implementation of `Repository` for Git repositories."""

    def __init__(self, path: Path, src="src"):
        super(SrcRepository, self).__init__(path, tool=None)

    @property
    def name(self):
        return "src"

    @property
    def head_ref(self):
        pass

    @property
    def base_ref(self):
        pass

    def base_ref_as_hg(self):
        pass

    @property
    def branch(self):
        pass

    @property
    def has_git_cinnabar(self):
        pass

    def get_commit_time(self):
        pass

    def sparse_checkout_present(self):
        pass

    def get_user_email(self):
        pass

    def get_upstream(self):
        pass

    def get_changed_files(self, diff_filter="ADM", mode="unstaged", rev=None):
        return []

    def get_outgoing_files(self, diff_filter="ADM", upstream=None):
        return []

    def add_remove_files(self, *paths: Union[str, Path], force: bool = False):
        pass

    def forget_add_remove_files(self, *paths: Union[str, Path]):
        pass

    def git_ignore(self, path):
        """This function reads the mozilla-central/.gitignore file and creates a
        list of the patterns to ignore
        """
        ignore = []
        f = open(path + "/.gitignore", "r")
        while True:
            line = f.readline()
            if not line:
                break
            if line.startswith("#"):
                pass
            elif line.strip() and line not in ["\r", "\r\n"]:
                ignore.append(line.strip().lstrip("/"))
        f.close()
        return ignore

    def get_files(self, path):
        """This function gets all files in your source folder e.g mozilla-central
        and creates a list of that
        """
        res = []
        # move away the .git or .hg folder from path to more easily test in a hg/git repo
        for root, dirs, files in os.walk(self.path):
            base = os.path.relpath(root, self.path)
            for name in files:
                res.append(os.path.join(base, name))
        return res

    def get_tracked_files_finder(self, path):
        """Get files, similar to 'hg files -0' or 'git ls-files -z', thats why
        we read the .gitignore file for patterns to ignore.
        Speed could probably be improved.
        """
        import fnmatch

        files = list(
            p.replace("\\", "/").replace("./", "") for p in self.get_files(path) if p
        )
        files.sort()
        ig = self.git_ignore(path)
        mat = []
        for i in ig:
            x = fnmatch.filter(files, i)
            if x:
                mat = mat + x
        match = list(set(files) - set(mat))
        match.sort()
        if len(match) == 0:
            return None
        else:
            return FileListFinder(match)

    def working_directory_clean(self, untracked=False, ignored=False):
        pass

    def clean_directory(self, path: Union[str, Path]):
        pass

    def update(self, ref):
        pass

    def push_to_try(
        self,
        message: str,
        changed_files: Dict[str, str] = {},
        allow_log_capture: bool = False,
    ):
        pass

    def set_config(self, name, value):
        pass

    def get_last_modified_time_for_file(self, path: Path):
        """Return last modified in VCS time for the specified file."""
        raise MissingVCSTool
