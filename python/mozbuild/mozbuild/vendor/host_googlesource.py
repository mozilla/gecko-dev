# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import base64

import requests

from mozbuild.vendor.host_base import BaseHost


class GoogleSourceHost(BaseHost):
    def upstream_commit(self, revision):
        """Query for a git commit and timestamp."""
        url = "/".join(
            [self.manifest["vendoring"]["url"], "+", revision + "?format=JSON"]
        )
        req = requests.get(url)
        req.raise_for_status()
        try:
            info = req.json()
        except ValueError:
            # As of 2017 May, googlesource sends 4 garbage characters
            # at the beginning of the json response. Work around this.
            # https://bugs.chromium.org/p/chromium/issues/detail?id=718550
            import json

            info = json.loads(req.text[4:])
        return (info["commit"], info["committer"]["time"])

    def upstream_snapshot(self, revision):
        return "/".join(
            [self.manifest["vendoring"]["url"], "+archive", revision + ".tar.gz"]
        )

    def upstream_path_to_file(self, revision, filepath):
        return (
            "/".join([self.manifest["vendoring"]["url"], "+", revision, filepath])
            + "?format=TEXT"
        )

    def _transform_single_file_to_destination(self, from_file, destination):
        # googlesource only supports a base64 encoded raw file
        with open(destination, "wb") as destFile:
            destFile.write(base64.b64decode(from_file.read()))
