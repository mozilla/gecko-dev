# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


_ANDROID_TASK_NAME_PREFIX = "android-"


@transforms.add
def set_component_attribute(config, tasks):
    for task in tasks:
        component_name = task.pop("component", None)
        if not component_name:
            task_name = task["name"]
            if task_name.startswith(_ANDROID_TASK_NAME_PREFIX):
                component_name = task_name[len(_ANDROID_TASK_NAME_PREFIX) :]
            else:
                raise NotImplementedError(
                    f"Cannot determine component name from task {task_name}"
                )

        attributes = task.setdefault("attributes", {})
        attributes["component"] = component_name

        yield task


@transforms.add
def define_ui_test_command_line(config, tasks):
    def apk_path(component_fragment, variant, apk_filename):
        return os.path.join(
            "/builds/worker/workspace/obj-build",
            "gradle/build",
            f"mobile/android/android-components/{component_fragment}",
            f"outputs/apk/{variant}",
            apk_filename,
        )

    for task in tasks:
        component = task["attributes"]["component"]
        flank_config = "components/arm.yml"

        apk_app, apk_test = None, None

        if component == "samples-browser":
            # Case 2: Exact match for "samples-browser" – gecko paths with "-debug"
            apk_app = apk_path(
                "samples/browser", "gecko/debug", "samples-browser-gecko-debug.apk"
            )
            apk_test = apk_path(
                "samples/browser",
                "androidTest/gecko/debug",
                "samples-browser-gecko-debug-androidTest.apk",
            )

        elif component.startswith("samples-"):
            # Case 3: Other samples-* (e.g., samples-glean)
            sample = component.replace("samples-", "")
            apk_app = apk_path(
                f"samples/{sample}", "debug", f"samples-{sample}-debug.apk"
            )
            apk_test = apk_path(
                f"samples/{sample}",
                "androidTest/debug",
                f"samples-{sample}-debug-androidTest.apk",
            )

        elif "-" in component:
            # Case 1a: Component with dash (e.g., feature-share → components/feature/share)
            category, submodule = component.split("-", 1)
            apk_app = apk_path(
                "samples/browser", "gecko/debug", "samples-browser-gecko-debug.apk"
            )
            apk_test = apk_path(
                f"components/{category}/{submodule}",
                "androidTest/debug",
                f"{component}-debug-androidTest.apk",
            )

        else:
            # Case 1b: Component with no dash (e.g., browser → components/browser/engine-gecko)
            apk_app = apk_path(
                "samples/browser", "gecko/debug", "samples-browser-gecko-debug.apk"
            )
            apk_test = apk_path(
                f"components/{component}/engine-gecko",
                "androidTest/debug",
                "browser-engine-gecko-debug-androidTest.apk",
            )

        run = task.setdefault("run", {})
        post_gradlew = run.setdefault("post-gradlew", [])
        post_gradlew.append(
            [
                "python3",
                "taskcluster/scripts/tests/test-lab.py",
                flank_config,
                apk_app,
                "--apk_test",
                apk_test,
            ]
        )

        yield task


@transforms.add
def define_treeherder_symbol(config, tasks):
    for task in tasks:
        treeherder = task.setdefault("treeherder")
        treeherder.setdefault("symbol", f"{task['attributes']['component']}(unit)")

        yield task


@transforms.add
def define_description(config, tasks):
    for task in tasks:
        task.setdefault(
            "description",
            f"Run unit/ui tests on device for {task['attributes']['component']}",
        )
        yield task
