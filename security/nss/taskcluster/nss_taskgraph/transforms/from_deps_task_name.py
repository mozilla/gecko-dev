# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.set_name import set_name_strip_kind

transforms = TransformSequence()


@transforms.add
def set_task_name(config, tasks):
    # set the name of tasks created by the from_deps transforms by appending
    # the primary dependency's label to the name from kind.yml
    for task in tasks:
        primary_kind = task["attributes"]["primary-kind-dependency"]
        primary_dep = [dep for dep in config.kind_dependencies_tasks.values() if dep.label in task["dependencies"].values()][0]
        task["name"] += f"-{set_name_strip_kind(config, [], primary_dep, primary_kind)}"
        yield task
