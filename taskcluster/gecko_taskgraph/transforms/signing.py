# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.keyed_by import evaluate_keyed_by
from taskgraph.util.schema import Schema, taskref_or_string
from voluptuous import Optional, Required

from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job
from gecko_taskgraph.util.scriptworker import (
    add_scope_prefix,
    get_signing_cert_scope_per_platform,
)

transforms = TransformSequence()

signing_description_schema = Schema(
    {
        # Artifacts from dep task to sign - Sync with taskgraph/transforms/task.py
        # because this is passed directly into the signingscript worker
        Required("upstream-artifacts"): [
            {
                # taskId of the task with the artifact
                Required("taskId"): taskref_or_string,
                # type of signing task (for CoT)
                Required("taskType"): str,
                # Paths to the artifacts to sign
                Required("paths"): [str],
                # Signing formats to use on each of the paths
                Required("formats"): [str],
            }
        ],
        # attributes for this task
        Optional("attributes"): {str: object},
        # unique label to describe this signing task, defaults to {dep.label}-signing
        Optional("label"): str,
        # treeherder is allowed here to override any defaults we use for signing.  See
        # taskcluster/gecko_taskgraph/transforms/task.py for the schema details, and the
        # below transforms for defaults of various values.
        Optional("treeherder"): task_description_schema["treeherder"],
        # Routes specific to this task, if defined
        Optional("routes"): [str],
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Required("dependencies"): task_description_schema["dependencies"],
        # Optional control for how long a task may run (aka maxRunTime)
        Optional("max-run-time"): int,
        Optional("extra"): {str: object},
        # Max number of partner repacks per chunk
        Optional("repacks-per-chunk"): int,
        # Override the default priority for the project
        Optional("priority"): task_description_schema["priority"],
        Optional("task-from"): task_description_schema["task-from"],
    }
)


def get_locales_description(attributes, default):
    """Returns the [list] of locales for task description usage"""
    chunk_locales = attributes.get("chunk_locales")
    if chunk_locales:
        return ", ".join(chunk_locales)
    return attributes.get("locale", default)


@transforms.add
def delete_name(config, jobs):
    """Delete the 'name' key if it exists, we don't use it."""
    for job in jobs:
        if "name" in job:
            del job["name"]
        yield job


transforms.add_validate(signing_description_schema)


@transforms.add
def add_requirements_link(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job
        requirements_path = evaluate_keyed_by(
            config.graph_config["mac-signing"]["mac-requirements"],
            "mac requirements",
            {
                "platform": dep_job.attributes.get("build_platform"),
            },
        )
        if requirements_path:
            job["requirements-plist-url"] = config.params.file_url(
                requirements_path,
            )
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job
        attributes = dep_job.attributes

        signing_format_scopes = []
        formats = set()
        for artifacts in job["upstream-artifacts"]:
            for f in artifacts["formats"]:
                formats.add(f)  # Add each format only once

        is_shippable = dep_job.attributes.get("shippable", False)
        build_platform = dep_job.attributes.get("build_platform")
        assert build_platform
        treeherder = None
        if "partner" not in config.kind and "eme-free" not in config.kind:
            treeherder = job.get("treeherder", {})

            dep_th_platform = (
                dep_job.task.get("extra", {})
                .get("treeherder", {})
                .get("machine", {})
                .get("platform", "")
            )
            build_type = dep_job.attributes.get("build_type")
            treeherder.setdefault(
                "platform",
                _generate_treeherder_platform(
                    dep_th_platform, build_platform, build_type
                ),
            )

            # ccov builds are tier 2, so they cannot have tier 1 tasks
            # depending on them.
            treeherder.setdefault(
                "tier",
                dep_job.task.get("extra", {}).get("treeherder", {}).get("tier", 1),
            )
            treeherder.setdefault(
                "symbol",
                _generate_treeherder_symbol(
                    dep_job.task.get("extra", {}).get("treeherder", {}).get("symbol")
                ),
            )
            treeherder.setdefault("kind", "build")

        label = job["label"]
        description = (
            "Signing of locale(s) '{locale}' for build '"
            "{build_platform}/{build_type}'".format(
                locale=get_locales_description(attributes, "en-US"),
                build_platform=build_platform,
                build_type=attributes.get("build_type"),
            )
        )

        attributes = (
            job["attributes"]
            if job.get("attributes")
            else copy_attributes_from_dependent_job(dep_job)
        )
        attributes["signed"] = True

        if "linux" in build_platform:
            attributes["release_artifacts"] = ["public/build/KEY"]

        if dep_job.attributes.get("chunk_locales"):
            # Used for l10n attribute passthrough
            attributes["chunk_locales"] = dep_job.attributes.get("chunk_locales")

        signing_cert_scope = get_signing_cert_scope_per_platform(
            build_platform, is_shippable, config
        )
        worker_type_alias = "linux-signing" if is_shippable else "linux-depsigning"
        task = {
            "label": label,
            "description": description,
            "worker": {
                "implementation": "scriptworker-signing",
                "upstream-artifacts": job["upstream-artifacts"],
                "max-run-time": job.get("max-run-time", 3600),
            },
            "scopes": [signing_cert_scope] + signing_format_scopes,
            "dependencies": job["dependencies"],
            "attributes": attributes,
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "optimization": dep_job.optimization,
            "routes": job.get("routes", []),
            "shipping-product": job.get("shipping-product"),
            "shipping-phase": job.get("shipping-phase"),
        }
        if dep_job.kind in task["dependencies"]:
            task["if-dependencies"] = [dep_job.kind]

        # build-mac-{signing,notarization} uses signingscript instead of iscript
        if "macosx" in build_platform and config.kind.endswith("-mac-notarization"):
            task["scopes"] = [
                add_scope_prefix(config, "signing:cert:release-apple-notarization")
            ]
            task["description"] = (
                "Notarization of '{}' locales for build '{}/{}'".format(
                    get_locales_description(attributes, "en-US"),
                    build_platform,
                    attributes.get("build_type"),
                )
            )
            task["retries"] = 0
        elif "macosx" in build_platform:
            # iscript overrides
            task["worker"]["mac-behavior"] = "mac_sign_and_pkg"

            worker_type_alias_map = {
                "linux-depsigning": "mac-depsigning",
                "linux-signing": "mac-signing",
            }
            assert worker_type_alias in worker_type_alias_map, (
                "Make sure to adjust the below worker_type_alias logic for "
                "mac if you change the signing workerType aliases!"
                f" ({worker_type_alias} not found in mapping)"
            )
            worker_type_alias = worker_type_alias_map[worker_type_alias]
            for attr in ("entitlements-url", "requirements-plist-url"):
                if job.get(attr):
                    task["worker"][attr] = job[attr]

        task["worker-type"] = worker_type_alias
        if treeherder:
            task["treeherder"] = treeherder
        if job.get("extra"):
            task["extra"] = job["extra"]
        # we may have reduced the priority for partner jobs, otherwise task.py will set it
        if job.get("priority"):
            task["priority"] = job["priority"]

        yield task


def _generate_treeherder_platform(dep_th_platform, build_platform, build_type):
    if "-pgo" in build_platform:
        actual_build_type = "pgo"
    elif "-ccov" in build_platform:
        actual_build_type = "ccov"
    else:
        actual_build_type = build_type
    return f"{dep_th_platform}/{actual_build_type}"


def _generate_treeherder_symbol(build_symbol):
    symbol = build_symbol + "s"
    return symbol
