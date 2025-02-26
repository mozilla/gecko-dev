# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def resolve_platform_differences(config, tasks):
    for task in tasks:
        for key in ("worker-type", "run.command", "scopes", "worker.artifacts", "worker.taskcluster-proxy", "worker.max-run-time"):
            resolve_keyed_by(task, key, task["name"], platform=task["attributes"]["build_platform"])
        yield task


@transforms.add
def add_env_vars(config, tasks):
    for task in tasks:
        env = task["worker"].setdefault("env", {})
        if task["attributes"]["build_platform"].startswith(("mac", "windows")):
            env.update({"DOMSUF": "localdomain", "HOST": "localhost"})

        if task["attributes"]["build_platform"].startswith("mac"):
            env.update({"NSS_TASKCLUSTER_MAC": "1"})

        if config.params["try_options"].get("allow_nspr_patch"):
            env.update({"ALLOW_NSPR_PATCH": "1"})

        yield task


@transforms.add
def remove_docker_image_for_gw(config, tasks):
    for task in tasks:
        if task["attributes"]["build_platform"].startswith(("mac", "windows")):
            task["worker"].pop("docker-image", None)
        yield task
