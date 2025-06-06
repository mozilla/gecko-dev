# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import logging
import os
import re

from voluptuous import Optional, Required

import taskgraph
from taskgraph.transforms.base import TransformSequence
from taskgraph.util import json
from taskgraph.util.docker import create_context_tar, generate_context_hash
from taskgraph.util.schema import Schema

from .task import task_description_schema

logger = logging.getLogger(__name__)

CONTEXTS_DIR = "docker-contexts"

DIGEST_RE = re.compile("^[0-9a-f]{64}$")

IMAGE_BUILDER_IMAGE = (
    "mozillareleases/image_builder:5.1.0"
    "@sha256:"
    "7fe70dcedefffffa03237ba5d456d42e0d7461de066db3f7a7c280a104869cd5"
)

transforms = TransformSequence()

docker_image_schema = Schema(
    {
        # Name of the docker image.
        Required("name"): str,
        # Name of the parent docker image.
        Optional("parent"): str,
        # Treeherder symbol.
        Optional("symbol"): str,
        # relative path (from config.path) to the file the docker image was defined
        # in.
        Optional("task-from"): str,
        # Arguments to use for the Dockerfile.
        Optional("args"): {str: str},
        # Name of the docker image definition under taskcluster/docker, when
        # different from the docker image name.
        Optional("definition"): str,
        # List of package tasks this docker image depends on.
        Optional("packages"): [str],
        Optional(
            "index",
            description="information for indexing this build so its artifacts can be discovered",
        ): task_description_schema["index"],
        Optional(
            "cache",
            description="Whether this image should be cached based on inputs.",
        ): bool,
    }
)


transforms.add_validate(docker_image_schema)


@transforms.add
def fill_template(config, tasks):
    available_packages = set()
    for task in config.kind_dependencies_tasks.values():
        if task.kind != "packages":
            continue
        name = task.label.replace("packages-", "")
        available_packages.add(name)

    context_hashes = {}

    if not taskgraph.fast and config.write_artifacts:
        if not os.path.isdir(CONTEXTS_DIR):
            os.makedirs(CONTEXTS_DIR)

    for task in tasks:
        image_name = task.pop("name")
        job_symbol = task.pop("symbol", None)
        args = task.pop("args", {})
        definition = task.pop("definition", image_name)
        packages = task.pop("packages", [])
        parent = task.pop("parent", None)

        for p in packages:
            if p not in available_packages:
                raise Exception(
                    f"Missing package job for {config.kind}-{image_name}: {p}"
                )

        if not taskgraph.fast:
            context_path = os.path.join("taskcluster", "docker", definition)
            topsrcdir = os.path.dirname(config.graph_config.taskcluster_yml)
            if config.write_artifacts:
                context_file = os.path.join(CONTEXTS_DIR, f"{image_name}.tar.gz")
                logger.info(f"Writing {context_file} for docker image {image_name}")
                context_hash = create_context_tar(
                    topsrcdir,
                    context_path,
                    context_file,
                    args,
                )
            else:
                context_hash = generate_context_hash(topsrcdir, context_path, args)
        else:
            if config.write_artifacts:
                raise Exception("Can't write artifacts if `taskgraph.fast` is set.")
            context_hash = "0" * 40
        digest_data = [context_hash]
        digest_data += [json.dumps(args, sort_keys=True)]
        context_hashes[image_name] = context_hash

        description = f"Build the docker image {image_name} for use by dependent tasks"

        args["DOCKER_IMAGE_PACKAGES"] = " ".join(f"<{p}>" for p in packages)

        # Adjust the zstandard compression level based on the execution level.
        # We use faster compression for level 1 because we care more about
        # end-to-end times. We use slower/better compression for other levels
        # because images are read more often and it is worth the trade-off to
        # burn more CPU once to reduce image size.
        zstd_level = "3" if int(config.params["level"]) == 1 else "10"

        expires = config.graph_config._config.get("task-expires-after", "28 days")

        # include some information that is useful in reconstructing this task
        # from JSON
        taskdesc = {
            "label": "docker-image-" + image_name,
            "description": description,
            "attributes": {
                "image_name": image_name,
                "artifact_prefix": "public",
            },
            "always-target": True,
            "expires-after": expires if config.params.is_try() else "1 year",
            "scopes": [],
            "run-on-projects": [],
            "worker-type": "images",
            "worker": {
                "implementation": "docker-worker",
                "os": "linux",
                "artifacts": [
                    {
                        "type": "file",
                        "path": "/workspace/image.tar.zst",
                        "name": "public/image.tar.zst",
                    }
                ],
                "env": {
                    "CONTEXT_TASK_ID": {"task-reference": "<decision>"},
                    "CONTEXT_PATH": f"public/docker-contexts/{image_name}.tar.gz",
                    "HASH": context_hash,
                    "PROJECT": config.params["project"],
                    "IMAGE_NAME": image_name,
                    "DOCKER_IMAGE_ZSTD_LEVEL": zstd_level,
                    "DOCKER_BUILD_ARGS": {
                        "task-reference": json.dumps(args),
                    },
                    "VCS_BASE_REPOSITORY": config.params["base_repository"],
                    "VCS_HEAD_REPOSITORY": config.params["head_repository"],
                    "VCS_HEAD_REV": config.params["head_rev"],
                    "VCS_REPOSITORY_TYPE": config.params["repository_type"],
                },
                "chain-of-trust": True,
                "max-run-time": 7200,
            },
        }
        if "index" in task:
            taskdesc["index"] = task["index"]
        if job_symbol:
            taskdesc["treeherder"] = {
                "symbol": job_symbol,
                "platform": "taskcluster-images/opt",
                "kind": "other",
                "tier": 1,
            }

        worker = taskdesc["worker"]

        worker["docker-image"] = IMAGE_BUILDER_IMAGE
        digest_data.append(f"image-builder-image:{IMAGE_BUILDER_IMAGE}")

        if packages:
            deps = taskdesc.setdefault("dependencies", {})
            for p in sorted(packages):
                deps[p] = f"packages-{p}"

        if parent:
            deps = taskdesc.setdefault("dependencies", {})
            deps["parent"] = f"docker-image-{parent}"
            worker["env"]["PARENT_TASK_ID"] = {
                "task-reference": "<parent>",
            }

        if task.get("cache", True) and not taskgraph.fast:
            taskdesc["cache"] = {
                "type": "docker-images.v2",
                "name": image_name,
                "digest-data": digest_data,
            }

        yield taskdesc
