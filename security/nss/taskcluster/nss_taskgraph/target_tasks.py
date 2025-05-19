# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.target_tasks import register_target_task, target_tasks_default


def filter_build_type(build_types, task):
    if "o" in build_types and "opt" in task.attributes["build_type"]:
        return True
    if "d" in build_types and "debug" in task.attributes["build_type"]:
        return True


PLATFORM_ALIASES = {
    "aarch64-make": "aarch64",
    "linux": "linux32",
    "linux-fuzz": "linux32",
    "linux64-fips": "linux64",
    "linux64-fuzz": "linux64",
    "linux64-make": "linux64",
    "linux-make": "linux32",
    "win64-make": "windows2022-64",
    "win-make": "windows2022-32",
    "win64": "windows2022-64",
    "win": "windows2022-32",
    "mac": "macosx64",
}


def filter_platform(platform, task):
    if "build_platform" not in task.attributes:
        return False
    if platform == "all":
        return True
    task_platform = task.attributes["build_platform"]
    # Check the platform name.
    keep = task_platform == PLATFORM_ALIASES.get(platform, platform)
    # Additional checks.
    if platform == "linux64-fips":
        keep &= task.attributes["fips"]
    elif (
        platform == "linux64-make"
        or platform == "linux-make"
        or platform == "win64-make"
        or platform == "win-make"
        or platform == "aarch64-make"
    ):
        keep &= task.attributes["make"]
    elif platform == "linux64-fuzz" or platform == "linux-fuzz":
        keep &= task.attributes["fuzz"]
    return keep


def filter_try_syntax(options, task):
    symbol = task.task["extra"]["treeherder"]["symbol"].lower()
    group = task.task["extra"]["treeherder"].get("groupSymbol", "").lower()

    # Filter tools. We can immediately return here as those
    # are not affected by platform or build type selectors.
    if task.kind == "tools":
        return any(t in options["tools"] for t in ["all", symbol])

    # Filter unit tests.
    if task.kind == "test":
        tests = {"all", symbol}
        if group in ("cipher", "ssl"):
            tests.add(group)
        if not any(t in options["unittests"] for t in tests):
            return False

    # Filter extra builds.
    if group == "builds" and not options["extra"]:
        return False

    # Filter by platform.
    if not any(filter_platform(platform, task) for platform in options["platforms"]):
        return False

    # Finally, filter by build type.
    return filter_build_type(options["builds"], task)


@register_target_task("nss_try_tasks")
def target_tasks_try(full_task_graph, parameters, graph_config):
    if not parameters["try_options"]:
        return target_tasks_default(full_task_graph, parameters, graph_config)
    return [
        t.label
        for t in full_task_graph
        if filter_try_syntax(parameters["try_options"], t)
    ]
