# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the partials task into an actual task description.
"""
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.treeherder import inherit_treeherder_from_dep

from gecko_taskgraph.util.partials import get_builds
from gecko_taskgraph.util.platforms import architecture

transforms = TransformSequence()


# TODO: Add cert verify
# def identify_desired_signing_keys(project, product):
#     if project in ["mozilla-central", "comm-central", "larch", "pine"]:
#         return "nightly"
#     if project == "mozilla-beta":
#         if product == "devedition":
#             return "nightly"
#         return "release"
#     if (
#         project in ["mozilla-release", "comm-release", "comm-beta"]
#         or project.startswith("mozilla-esr")
#         or project.startswith("comm-esr")
#     ):
#         return "release"
#     return "dep1"


@transforms.add
def make_task_description(config, tasks):
    # If no balrog release history, then don't generate partials
    if not config.params.get("release_history"):
        return
    for task in tasks:
        dep_task = get_primary_dependency(config, task)
        assert dep_task

        # attributes = copy_attributes_from_dependent_job(dep_job)
        locale = task["attributes"].get("locale")

        build_locale = locale or "en-US"

        build_platform = task["attributes"]["build_platform"]
        builds = get_builds(
            config.params["release_history"], build_platform, build_locale
        )

        # If the list is empty there's no available history for this platform
        # and locale combination, so we can't build any partials.
        if not builds:
            continue

        locale_suffix = ""
        if locale:
            locale_suffix = f"{locale}/"
        artifact_path = f"{locale_suffix}target.complete.mar"

        # Fetches from upstream repackage task
        task["fetches"][dep_task.kind] = [artifact_path]

        extra_params = [f"--target=/builds/worker/artifacts/{locale_suffix}"]

        for build in sorted(builds):
            url = builds[build]["mar_url"]
            extra_params.append(f"--from_url={url}")

        to_mar_path = None
        for artifact in dep_task.attributes["release_artifacts"]:
            if artifact.endswith(".complete.mar"):
                to_mar_path = artifact
                break

        task["worker"]["env"]["TO_MAR_TASK_ID"] = {
            "task-reference": f"<{dep_task.kind}>"
        }
        extra_params.extend(
            [
                f"--arch={architecture(build_platform)}",
                f"--locale={build_locale}",
                # This isn't a great approach to resolving the source URL of to_mar, but it's the same as
                # It's only being used to fill manifest.json information, the actual file is downloaded via run-task
                f"--to_mar_url=https://firefox-ci-tc.services.mozilla.com/api/queue/v1/task/$TO_MAR_TASK_ID/artifacts/{to_mar_path}",
            ]
        )
        # Add a space from the last command + list of extra_params
        task["run"]["command"] += " " + " ".join(extra_params)

        task["description"] = (
            f"Partials task for locale '{build_locale}' for build '{build_platform}'"
        )

        task["treeherder"] = inherit_treeherder_from_dep(task, dep_task)
        task["treeherder"]["symbol"] = f"pz({locale or 'N'})"

        task["worker"]["env"]["MAR_CHANNEL_ID"] = task["attributes"]["mar-channel-id"]
        yield task
