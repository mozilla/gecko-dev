# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
from io import BytesIO

from taskgraph.util.docker import create_context_tar, docker_image, stream_context_tar

from gecko_taskgraph.util import docker

from . import GECKO


def build_context(name, outputFile, args=None):
    """Build a context.tar for image with specified name."""
    if not name:
        raise ValueError("must provide a Docker image name")
    if not outputFile:
        raise ValueError("must provide a outputFile")

    image_dir = docker.image_path(name)
    if not os.path.isdir(image_dir):
        raise Exception("image directory does not exist: %s" % image_dir)

    create_context_tar(GECKO, image_dir, outputFile, args=args)


def build_image(name, tag, args=None):
    """Build a Docker image of specified name.

    Output from image building process will be printed to stdout.
    """
    if not name:
        raise ValueError("must provide a Docker image name")

    image_dir = docker.image_path(name)
    if not os.path.isdir(image_dir):
        raise Exception("image directory does not exist: %s" % image_dir)

    tag = tag or docker_image(name, by_tag=True)

    buf = BytesIO()
    stream_context_tar(GECKO, image_dir, buf, args)
    docker.post_to_docker(buf.getvalue(), "/build", nocache=1, t=tag)

    print(f"Successfully built {name} and tagged with {tag}")

    if tag.endswith(":latest"):
        print("*" * 50)
        print("WARNING: no VERSION file found in image directory.")
        print("Image is not suitable for deploying/pushing.")
        print("Create an image suitable for deploying/pushing by creating")
        print("a VERSION file in the image directory.")
        print("*" * 50)
