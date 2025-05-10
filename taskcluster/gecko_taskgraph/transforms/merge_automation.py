# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the update generation task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def handle_keyed_by(config, tasks):
    """Resolve fields that can be keyed by platform, etc."""
    if "merge_config" not in config.params:
        return
    merge_config = config.params["merge_config"]
    fields = [
        "routes",
        "scopes",
        "worker-type",
        "worker.l10n-bump-info",
        "worker.lando-repo",
    ]
    for task in tasks:
        for field in fields:
            resolve_keyed_by(
                task,
                field,
                item_name=task["name"],
                **{
                    "project": config.params["project"],
                    "release-type": config.params["release_type"],
                    "behavior": merge_config["behavior"],
                    "level": config.params["level"],
                }
            )
        yield task


@transforms.add
def update_labels(config, tasks):
    for task in tasks:
        merge_config = config.params["merge_config"]
        task["label"] = "merge-{}".format(merge_config["behavior"])
        treeherder = task.get("treeherder", {})
        treeherder["symbol"] = "Rel({})".format(merge_config["behavior"])
        task["treeherder"] = treeherder
        yield task


@transforms.add
def add_payload_config(config, tasks):
    for task in tasks:
        if "merge_config" not in config.params:
            break
        merge_config = config.params["merge_config"]
        worker = task["worker"]
        worker["merge-info"] = config.graph_config["merge-automation"]["behaviors"][
            merge_config["behavior"]
        ]

        if "l10n-bump-info" in worker and worker["l10n-bump-info"] is None:
            del worker["l10n-bump-info"]

        # Override defaults, useful for testing.
        for field in [
            "from-branch",
            "to-branch",
            "fetch-version-from",
            "lando-repo",
        ]:
            if merge_config.get(field):
                worker["merge-info"][field] = merge_config[field]

        worker["force-dry-run"] = merge_config["force-dry-run"]
        yield task
