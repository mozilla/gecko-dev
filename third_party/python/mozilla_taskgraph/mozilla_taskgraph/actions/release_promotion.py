from taskgraph.decision import taskgraph_decision
from taskgraph.parameters import Parameters
from taskgraph.taskgraph import TaskGraph
from taskgraph.util.python_path import find_object
from taskgraph.util.taskcluster import get_artifact
from taskgraph.util.taskgraph import (
    find_decision_task,
    find_existing_tasks_from_previous_kinds,
)

from mozilla_taskgraph.actions import make_action_available


@make_action_available(
    name="release-promotion",
    title="Release Promotion",
    symbol="${input.release_promotion_flavor}",
    description="Run a release promotion graph.",
    permission="release-promotion",
    order=500,
    context=[],
    available=lambda _: True,
    schema=lambda graph_config: {
        "type": "object",
        "properties": {
            "release_promotion_flavor": {
                "type": "string",
                "description": "The flavor of release promotion to perform.",
                "default": "<REPLACE ME>",
                "enum": sorted(graph_config["release-promotion"]["flavors"].keys()),
            },
            "build_number": {
                "type": "integer",
                "default": 1,
                "minimum": 1,
                "title": "The release build number",
                "description": (
                    "The release build number. Starts at 1 per "
                    "release version, and increments on rebuild."
                ),
            },
            "version": {
                "type": "string",
                "description": (
                    "Optional: override the version for release promotion. "
                    "Occasionally we'll land a taskgraph fix in a later "
                    "commit, but want to act on a build from a previous "
                    "commit. If a version bump has landed in the meantime, "
                    "relying on the in-tree version will break things."
                ),
                "default": "",
            },
            "do_not_optimize": {
                "type": "array",
                "description": (
                    "Optional: a list of labels to avoid optimizing out "
                    "of the graph (to force a rerun)."
                ),
                "items": {
                    "type": "string",
                },
            },
            "rebuild_kinds": {
                "type": "array",
                "description": (
                    "Optional: an array of kinds to ignore from the previous "
                    "graph(s)."
                ),
                "default": graph_config["release-promotion"].get("rebuild-kinds", []),
                "items": {
                    "type": "string",
                },
            },
            "previous_graph_ids": {
                "type": "array",
                "description": (
                    "Optional: an array of taskIds of decision or action "
                    "tasks from the previous graph(s) to use to populate "
                    "our `previous_graph_kinds`."
                ),
                "items": {
                    "type": "string",
                },
            },
        },
        "required": [
            "release_promotion_flavor",
            "build_number",
        ],
    },
)
def release_promotion_action(parameters, graph_config, input, task_group_id, task_id):
    release_promotion_flavor = input["release_promotion_flavor"]
    promotion_config = graph_config["release-promotion"]["flavors"][
        release_promotion_flavor
    ]

    target_tasks_method = promotion_config["target-tasks-method"].format(
        project=parameters["project"]
    )
    rebuild_kinds = input.get(
        "rebuild_kinds", promotion_config.get("rebuild-kinds", [])
    )
    do_not_optimize = input.get(
        "do_not_optimize", promotion_config.get("do-not-optimize", [])
    )

    # make parameters read-write
    parameters = dict(parameters)
    # Build previous_graph_ids from ``previous_graph_ids`` or ``revision``.
    previous_graph_ids = input.get("previous_graph_ids")
    if not previous_graph_ids:
        previous_graph_ids = [find_decision_task(parameters, graph_config)]

    # Download parameters from the first decision task
    parameters = get_artifact(previous_graph_ids[0], "public/parameters.yml")

    # Download and combine full task graphs from each of the previous_graph_ids.
    # Sometimes previous relpro action tasks will add tasks, like partials,
    # that didn't exist in the first full_task_graph, so combining them is
    # important. The rightmost graph should take precedence in the case of
    # conflicts.
    combined_full_task_graph = {}
    for graph_id in previous_graph_ids:
        full_task_graph = get_artifact(graph_id, "public/full-task-graph.json")
        combined_full_task_graph.update(full_task_graph)
    _, combined_full_task_graph = TaskGraph.from_json(combined_full_task_graph)
    parameters["existing_tasks"] = find_existing_tasks_from_previous_kinds(
        combined_full_task_graph, previous_graph_ids, rebuild_kinds
    )
    parameters["do_not_optimize"] = do_not_optimize
    parameters["target_tasks_method"] = target_tasks_method
    parameters["build_number"] = int(input["build_number"])

    # When doing staging releases, we still want to reuse tasks from previous
    # graphs.
    parameters["optimize_target_tasks"] = True
    parameters["shipping_phase"] = input["release_promotion_flavor"]
    parameters["tasks_for"] = "action"

    if input["version"]:
        parameters["version"] = input["version"]
    else:
        if "version-parser" in graph_config:
            version_parser_objpath = graph_config["version-parser"]
        else:
            version_parser_objpath = "mozilla_taskgraph.version:default_parser"
        version_func = find_object(version_parser_objpath)
        parameters["version"] = version_func(parameters)

    # make parameters read-only
    parameters = Parameters(**parameters)

    taskgraph_decision({"root": graph_config.root_dir}, parameters=parameters)
