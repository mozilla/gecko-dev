# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re

from taskgraph.filter_tasks import filter_task
from taskgraph.parameters import Parameters
from taskgraph.target_tasks import get_method

from gecko_taskgraph.target_tasks import (
    filter_by_regex,
    filter_by_uncommon_try_tasks,
    filter_out_shippable,
    filter_unsupported_artifact_builds,
    target_tasks_default,
)


@filter_task("try_auto")
def target_tasks_try_auto(full_task_graph, parameters, graph_config):
    """Target the tasks which have indicated they should be run on autoland
    (rather than try) via the `run_on_projects` attributes.

    Should do the same thing as the `default` target tasks method.
    """
    params = dict(parameters)
    params["project"] = "autoland"
    params["target_tasks_method"] = "default"
    parameters = Parameters(**params)

    regex_filters = parameters["try_task_config"].get("tasks-regex")
    include_regexes = exclude_regexes = []
    if regex_filters:
        include_regexes = [re.compile(r) for r in regex_filters.get("include", [])]
        exclude_regexes = [re.compile(r) for r in regex_filters.get("exclude", [])]

    filtered_for_default = target_tasks_default(
        full_task_graph, parameters, graph_config
    )
    filtered_for_try_auto = [
        l
        for l, t in full_task_graph.tasks.items()
        if filter_by_uncommon_try_tasks(t.label)
        and filter_by_regex(t.label, include_regexes, mode="include")
        and filter_by_regex(t.label, exclude_regexes, mode="exclude")
        and filter_unsupported_artifact_builds(t, parameters)
        and filter_out_shippable(t)
    ]
    return list(set(filtered_for_default) & set(filtered_for_try_auto))


@filter_task("try_select_tasks")
def target_tasks_try_select(full_task_graph, parameters, graph_config):
    tasks = target_tasks_try_select_uncommon(full_task_graph, parameters, graph_config)
    return [l for l in tasks if filter_by_uncommon_try_tasks(l)]


@filter_task("try_select_tasks_uncommon")
def target_tasks_try_select_uncommon(full_task_graph, parameters, graph_config):
    from gecko_taskgraph.decision import PER_PROJECT_PARAMETERS

    if parameters["target_tasks_method"] != "default":
        # A parameter set using a custom target_tasks_method was explicitly
        # passed in to `./mach try` via `--parameters`. In this case, don't
        # override it or do anything special.
        tasks = get_method(parameters["target_tasks_method"])(
            full_task_graph, parameters, graph_config
        )
    else:
        # Union the tasks between autoland and mozilla-central as a sensible
        # default. This is likely the set of tasks that most users are
        # attempting to select from.
        projects = ("autoland", "mozilla-central")
        if parameters["project"] not in projects:
            projects = (parameters["project"],)

        tasks = set()
        for project in projects:
            params = dict(parameters)
            params["project"] = project
            parameters = Parameters(**params)

            try:
                target_tasks_method = PER_PROJECT_PARAMETERS[project][
                    "target_tasks_method"
                ]
            except KeyError:
                target_tasks_method = "default"

            tasks.update(
                get_method(target_tasks_method)(
                    full_task_graph, parameters, graph_config
                )
            )

    return sorted(tasks)
