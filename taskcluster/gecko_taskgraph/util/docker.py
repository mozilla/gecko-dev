# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import re
import sys
from collections.abc import Mapping
from urllib.parse import quote, urlencode, urlunparse

import requests
import requests_unixsocket
from mozbuild.util import memoize
from taskgraph.util.yaml import load_yaml

from .. import GECKO

IMAGE_DIR = os.path.join(GECKO, "taskcluster", "docker")


def docker_url(path, **kwargs):
    docker_socket = os.environ.get("DOCKER_SOCKET", "/var/run/docker.sock")
    return urlunparse(
        ("http+unix", quote(docker_socket, safe=""), path, "", urlencode(kwargs), "")
    )


def post_to_docker(tar, api_path, **kwargs):
    """POSTs a tar file to a given docker API path.

    The tar argument can be anything that can be passed to requests.post()
    as data (e.g. iterator or file object).
    The extra keyword arguments are passed as arguments to the docker API.
    """
    # requests-unixsocket doesn't honor requests timeouts
    # See https://github.com/msabramo/requests-unixsocket/issues/44
    # We have some large docker images that trigger the default timeout,
    # so we increase the requests-unixsocket timeout here.
    session = requests.Session()
    session.mount(
        requests_unixsocket.DEFAULT_SCHEME,
        requests_unixsocket.UnixAdapter(timeout=120),
    )
    req = session.post(
        docker_url(api_path, **kwargs),
        data=tar,
        stream=True,
        headers={"Content-Type": "application/x-tar"},
    )
    if req.status_code != 200:
        message = req.json().get("message")
        if not message:
            message = f"docker API returned HTTP code {req.status_code}"
        raise Exception(message)
    status_line = {}

    buf = b""
    for content in req.iter_content(chunk_size=None):
        if not content:
            continue
        # Sometimes, a chunk of content is not a complete json, so we cumulate
        # with leftovers from previous iterations.
        buf += content
        try:
            data = json.loads(buf)
        except Exception:
            continue
        buf = b""
        # data is sometimes an empty dict.
        if not data:
            continue
        # Mimick how docker itself presents the output. This code was tested
        # with API version 1.18 and 1.26.
        if "status" in data:
            if "id" in data:
                if sys.stderr.isatty():
                    total_lines = len(status_line)
                    line = status_line.setdefault(data["id"], total_lines)
                    n = total_lines - line
                    if n > 0:
                        # Move the cursor up n lines.
                        sys.stderr.write(f"\033[{n}A")
                    # Clear line and move the cursor to the beginning of it.
                    sys.stderr.write("\033[2K\r")
                    sys.stderr.write(
                        "{}: {} {}\n".format(
                            data["id"], data["status"], data.get("progress", "")
                        )
                    )
                    if n > 1:
                        # Move the cursor down n - 1 lines, which, considering
                        # the carriage return on the last write, gets us back
                        # where we started.
                        sys.stderr.write(f"\033[{n - 1}B")
                else:
                    status = status_line.get(data["id"])
                    # Only print status changes.
                    if status != data["status"]:
                        sys.stderr.write("{}: {}\n".format(data["id"], data["status"]))
                        status_line[data["id"]] = data["status"]
            else:
                status_line = {}
                sys.stderr.write("{}\n".format(data["status"]))
        elif "stream" in data:
            sys.stderr.write(data["stream"])
        elif "aux" in data:
            sys.stderr.write(repr(data["aux"]))
        elif "error" in data:
            sys.stderr.write("{}\n".format(data["error"]))
            # Sadly, docker doesn't give more than a plain string for errors,
            # so the best we can do to propagate the error code from the command
            # that failed is to parse the error message...
            errcode = 1
            m = re.search(r"returned a non-zero code: (\d+)", data["error"])
            if m:
                errcode = int(m.group(1))
            sys.exit(errcode)
        else:
            raise NotImplementedError(repr(data))
        sys.stderr.flush()


class ImagePathsMap(Mapping):
    """ImagePathsMap contains the mapping of Docker image names to their
    context location in the filesystem. The register function allows Thunderbird
    to define additional images under comm/taskcluster.
    """

    def __init__(self, config_path, image_dir=IMAGE_DIR):
        config = load_yaml(GECKO, config_path)
        self.__update_image_paths(config["tasks"], image_dir)

    def __getitem__(self, key):
        return self.__dict__[key]

    def __iter__(self):
        return iter(self.__dict__)

    def __len__(self):
        return len(self.__dict__)

    def __update_image_paths(self, jobs, image_dir):
        self.__dict__.update(
            {
                k: os.path.join(image_dir, v.get("definition", k))
                for k, v in jobs.items()
            }
        )

    def register(self, jobs_config_path, image_dir):
        """Register additional image_paths. In this case, there is no 'jobs'
        key in the loaded YAML as this file is loaded via jobs-from in kind.yml."""
        jobs = load_yaml(GECKO, jobs_config_path)
        self.__update_image_paths(jobs, image_dir)


image_paths = ImagePathsMap("taskcluster/kinds/docker-image/kind.yml")


def image_path(name):
    if name in image_paths:
        return image_paths[name]
    return os.path.join(IMAGE_DIR, name)


@memoize
def parse_volumes(image):
    """Parse VOLUME entries from a Dockerfile for an image."""
    volumes = set()

    path = image_path(image)

    with open(os.path.join(path, "Dockerfile"), "rb") as fh:
        for line in fh:
            line = line.strip()
            # We assume VOLUME definitions don't use ARGS.
            if not line.startswith(b"VOLUME "):
                continue

            v = line.split(None, 1)[1]
            if v.startswith(b"["):
                raise ValueError(
                    "cannot parse array syntax for VOLUME; "
                    "convert to multiple entries"
                )

            volumes |= {v.decode("utf-8") for v in v.split()}

    return volumes
