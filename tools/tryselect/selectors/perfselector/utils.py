# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import sys

TREEHERDER_REVISION_MATCHER = re.compile(
    r"remote:.*https://treeherder\.mozilla\.org/jobs\?.*revision=([\w]*)[ \t]*$"
)
HGMO_REVISION_MATCHER = re.compile(
    r"remote:.*/try/(?:rev/|pushloghtml\?changeset=)([\w]*)[ \t]*$"
)


class LogProcessor:
    def __init__(self):
        self.buf = ""
        self.stdout = sys.__stdout__
        self._treeherder_revision = None
        self._hg_revision = None

    @property
    def revision(self):
        return self._treeherder_revision or self._hg_revision

    def write(self, buf):
        while buf:
            try:
                newline_index = buf.index("\n")
            except ValueError:
                # No newline, wait for next call
                self.buf += buf
                break

            # Get data up to next newline and combine with previously buffered data
            data = self.buf + buf[: newline_index + 1]
            buf = buf[newline_index + 1 :]

            # Reset buffer then output line
            self.buf = ""
            if data.strip() == "":
                continue
            self.stdout.write(data.strip("\n") + "\n")

            # Check if a temporary commit was created and a Treeherder
            # link is found with the remote revision
            match = TREEHERDER_REVISION_MATCHER.match(data)
            if match:
                self._treeherder_revision = match.group(1)

            # Gather the hg revision in case the Treeherder one
            # is missing for some reason
            match = HGMO_REVISION_MATCHER.match(data)
            if match:
                # Last line found is the revision we want for HG patches
                self._hg_revision = match.group(1)
