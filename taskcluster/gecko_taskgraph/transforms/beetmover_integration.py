# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""


from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.schema import Schema
from voluptuous import Optional, Required

from gecko_taskgraph.transforms.beetmover import craft_release_properties
from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job
from gecko_taskgraph.util.scriptworker import (
    generate_beetmover_artifact_map,
    generate_beetmover_upstream_artifacts,
    get_beetmover_action_scope,
    get_beetmover_bucket_scope,
)

beetmover_description_schema = Schema(
    {
        Required("label"): str,
        Required("dependencies"): task_description_schema["dependencies"],
        Required("if-dependencies"): task_description_schema["if-dependencies"],
        Optional("treeherder"): task_description_schema["treeherder"],
        Required("run-on-projects"): task_description_schema["run-on-projects"],
        Optional("attributes"): task_description_schema["attributes"],
        Optional("task-from"): task_description_schema["task-from"],
    }
)

transforms = TransformSequence()


@transforms.add
def remove_name(config, jobs):
    for job in jobs:
        if "name" in job:
            del job["name"]
        yield job


transforms.add_validate(beetmover_description_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get("attributes", {}))

        treeherder = job.get("treeherder", {})
        dep_th_platform = (
            dep_job.task.get("extra", {})
            .get("treeherder", {})
            .get("machine", {})
            .get("platform", "")
        )
        treeherder.setdefault("platform", f"{dep_th_platform}/opt")
        treeherder.setdefault("tier", 2)
        treeherder.setdefault("kind", "build")
        treeherder.setdefault("symbol", "BM-int")
        label = job["label"]
        description = (
            "Beetmover submission for autoland-"
            "{build_platform}/{build_type}'".format(
                build_platform=attributes.get("build_platform"),
                build_type=attributes.get("build_type"),
            )
        )

        job["dependencies"].update(deepcopy(dep_job.dependencies))

        if job.get("locale"):
            attributes["locale"] = job["locale"]

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)

        task = {
            "label": label,
            "description": description,
            "worker-type": "beetmover",
            "scopes": [bucket_scope, action_scope],
            "dependencies": job["dependencies"],
            "if-dependencies": job.get("if-dependencies"),
            "attributes": attributes,
            "run-on-projects": job["run-on-projects"],
            "treeherder": treeherder,
        }

        yield task


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]

        worker = {
            "implementation": "beetmover",
            "release-properties": craft_release_properties(config, job),
            "upstream-artifacts": generate_beetmover_upstream_artifacts(
                config, job, platform, locale
            ),
            "artifact-map": generate_beetmover_artifact_map(
                config, job, platform=platform, locale=locale
            ),
        }

        job["worker"] = worker

        yield job
