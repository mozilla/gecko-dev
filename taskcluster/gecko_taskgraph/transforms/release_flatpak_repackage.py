# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.taskcluster import get_artifact_url

from gecko_taskgraph.util.attributes import release_level
from gecko_taskgraph.util.scriptworker import get_release_config

transforms = TransformSequence()


@transforms.add
def format(config, tasks):
    """Apply format substitution to worker.env and worker.command."""

    format_params = {
        "release_config": get_release_config(config),
        "config_params": config.params,
    }

    for task in tasks:
        format_params["task"] = task

        command = task.get("worker", {}).get("command", [])
        task["worker"]["command"] = [x.format(**format_params) for x in command]

        env = task.get("worker", {}).get("env", {})
        for k in env.keys():
            resolve_keyed_by(
                env,
                k,
                "flatpak envs",
                **{
                    "release-level": release_level(config.params["project"]),
                    "project": config.params["project"],
                },
            )
            task["worker"]["env"][k] = env[k].format(**format_params)

        yield task


@transforms.add
def add_desktop_file_url(config, tasks):
    """Add desktop file artifact url to task environment"""
    for task in tasks:
        for dep_task in config.kind_dependencies_tasks.values():
            if dep_task.label not in task["dependencies"]:
                continue
            if dep_task.kind != "repackage":
                continue
            env = task["worker"]["env"]
            assert "DESKTOP_FILE_URL" not in env
            env["DESKTOP_FILE_URL"] = {
                "task-reference": get_artifact_url(
                    f"<{dep_task.label}>", "public/build/target.flatpak.desktop"
                )
            }
        yield task
