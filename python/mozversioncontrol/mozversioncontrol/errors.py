# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


class MissingVCSTool(Exception):
    """Represents a failure to find a version control tool binary."""


class MissingVCSInfo(Exception):
    """Represents a general failure to resolve a VCS interface."""


class MissingConfigureInfo(MissingVCSInfo):
    """Represents error finding VCS info from configure data."""


class MissingVCSExtension(MissingVCSInfo):
    """Represents error finding a required VCS extension."""

    def __init__(self, ext):
        self.ext = ext
        msg = "Could not detect required extension '{}'".format(self.ext)
        super(MissingVCSExtension, self).__init__(msg)


class InvalidRepoPath(Exception):
    """Represents a failure to find a VCS repo at a specified path."""


class MissingUpstreamRepo(Exception):
    """Represents a failure to automatically detect an upstream repo."""


class CannotDeleteFromRootOfRepositoryException(Exception):
    """Represents that the code attempted to delete all files from the root of
    the repository, which is not permitted."""
