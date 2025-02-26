# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()

@transforms.add
def set_treeherder_symbol(config, tasks):
    for task in tasks:
        dep = config.kind_dependencies_tasks[task["dependencies"][task["attributes"]["primary-kind-dependency"]]]
        if dep.task["extra"]["treeherder"].get("groupSymbol"):
            task["treeherder"]["symbol"] = f'{dep.task["extra"]["treeherder"].get("groupSymbol")}({task["treeherder"]["symbol"]})'
        task["treeherder"]["platform"] = f'{task["attributes"]["build_platform"]}/{task["attributes"]["build_type"]}'
        yield task
