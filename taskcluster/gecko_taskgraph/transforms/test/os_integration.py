# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def maybe_setup_os_integration(config, tasks):
    if config.params["target_tasks_method"] != "os-integration":
        yield from tasks
        return

    for task in tasks:
        # Tags are ignored for raptor / talos. Marionette doesn't
        # support dynamic chunking.
        if task["suite"] in ("raptor", "talos", "marionette"):
            yield task
            continue

        if (
            task.get("test-manifest-loader", True) is not None
            and isinstance(task["chunks"], int)
            and task["chunks"] > 1
        ):
            task["chunks"] = "dynamic"

        env = task.setdefault("worker", {}).setdefault("env", {})
        env["MOZHARNESS_TEST_TAG"] = ["os_integration"]
        yield task
