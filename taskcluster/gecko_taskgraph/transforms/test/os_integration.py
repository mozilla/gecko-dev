# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_index_route(config, tasks):
    project = config.params["project"]
    if project != "mozilla-central":
        yield from tasks
        return

    index_base = f"gecko.v2.{project}.latest.test.os-integration"
    for task in tasks:
        routing_key = f"{index_base}.{task['test-platform']}.{task['test-name']}"
        if task["chunks"] > 1:
            routing_key = f"{routing_key}-{task['this-chunk']}"

        task.setdefault("routes", []).append(routing_key)
        yield task
