from taskgraph.transforms.task import payload_builder, taskref_or_string
from voluptuous import Any, Extra, Optional, Required

from mozilla_taskgraph.util.signed_artifacts import get_signed_artifacts


@payload_builder(
    "scriptworker-bitrise",
    schema={
        Required("bitrise"): {
            Required(
                "app", description="Name of Bitrise App to schedule workflows on."
            ): str,
            Required(
                "workflows",
                description="List of workflows to trigger on specified app. "
                "Can also be an object that maps workflow_ids to environment variables.",
            ): [
                Any(
                    # Workflow id - no special environment variable
                    str,
                    # Map of workflow id to permutations of environment variables
                    {str: [{str: taskref_or_string}]},
                )
            ],
            Optional(
                "artifact_prefix",
                description="Directory prefix to store artifacts. Set this to 'public' "
                "to create public artifacts.",
            ): str,
        },
        Extra: object,
    },
)
def build_bitrise_payload(config, task, task_def):
    bitrise = task["worker"]["bitrise"]
    task_def["tags"]["worker-implementation"] = "scriptworker"

    # Normalize environment variables to bitrise's format.
    workflow_permutations = {}
    for workflow in bitrise["workflows"]:
        if isinstance(workflow, str):
            # Empty environments
            continue
        for workflow_id, env_permutations in workflow.items():
            workflow_permutations.setdefault(workflow_id, [])
            for envs in env_permutations:
                workflow_permutations[workflow_id].append(
                    {
                        "environments": [
                            {"mapped_to": k, "value": v} for k, v in envs.items()
                        ]
                    }
                )

    def get_workflow_ids():
        ids = []
        for w in bitrise["workflows"]:
            if isinstance(w, str):
                ids.append(w)
            else:
                ids.extend(w.keys())
        ids = list(set(ids))  # Unique only
        ids.sort()  # sorted to allow for proper unit testing
        return ids

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    scopes = task_def.setdefault("scopes", [])
    scopes.append(f"{scope_prefix}:bitrise:app:{bitrise['app']}")
    scopes.extend(
        [f"{scope_prefix}:bitrise:workflow:{wf}" for wf in get_workflow_ids()]
    )

    def normref(ref, type="heads"):
        if ref:
            prefix = f"refs/{type}/"
            if ref.startswith(prefix):
                return ref[len(prefix) :]
            # The ref is a different type than the requested one, return None
            # to indicate this.
            elif ref.startswith("refs/"):
                return None
        return ref

    # Set some global_params implicitly from Taskcluster params.
    global_params = {
        "commit_hash": config.params["head_rev"],
        "branch_repo_owner": config.params["head_repository"],
    }

    if head_ref := normref(config.params["head_ref"]):
        global_params["branch"] = head_ref

    if head_tag := normref(config.params["head_tag"], type="tags"):
        global_params["tag"] = head_tag

    if commit_message := config.params.get("commit_message"):
        global_params["commit_message"] = commit_message

    if pull_request_number := config.params.get("pull_request_number"):
        global_params["pull_request_id"] = pull_request_number

    if config.params["tasks_for"] == "github-pull-request":
        global_params["pull_request_author"] = config.params["owner"]

        if base_ref := normref(config.params["base_ref"]):
            global_params["branch_dest"] = base_ref

        if base_repository := config.params["base_repository"]:
            global_params["branch_dest_repo_owner"] = base_repository

    task_def["payload"] = {"global_params": global_params}
    if workflow_permutations:
        task_def["payload"]["workflow_params"] = workflow_permutations

    if bitrise.get("artifact_prefix"):
        task_def["payload"]["artifact_prefix"] = bitrise["artifact_prefix"]


@payload_builder(
    "scriptworker-shipit",
    schema={
        Required("release-name"): str,
    },
)
def build_shipit_payload(config, task, task_def):
    worker = task["worker"]
    task_def["tags"]["worker-implementation"] = "scriptworker"
    task_def["payload"] = {"release_name": worker["release-name"]}


@payload_builder(
    "scriptworker-signing",
    schema={
        Required("signing-type"): str,
        # list of artifact URLs for the artifacts that should be signed
        Required("upstream-artifacts"): [
            {
                # taskId of the task with the artifact
                Required("taskId"): taskref_or_string,
                # type of signing task (for CoT)
                Required("taskType"): str,
                # Paths to the artifacts to sign
                Required("paths"): [str],
                # Signing formats to use on each of the paths
                Required("formats"): [str],
                # Only For MSI, optional for the signed Installer
                Optional("authenticode_comment"): str,
            }
        ],
        Optional("max-run-time"): int,
    },
)
def build_signing_payload(config, task, task_def):
    worker = task["worker"]

    task_def["payload"] = {
        "upstreamArtifacts": worker["upstream-artifacts"],
    }
    if "max-run-time" in worker:
        task_def["payload"]["maxRunTime"] = worker["max-run-time"]

    task_def.setdefault("tags", {})["worker-implementation"] = "scriptworker"

    formats = set()
    for artifacts in worker["upstream-artifacts"]:
        formats.update(artifacts["formats"])

    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    scopes = set(task_def.get("scopes", []))
    scopes.add(f"{scope_prefix}:signing:cert:{worker['signing-type']}")

    task_def["scopes"] = sorted(scopes)

    # Set release artifacts
    artifacts = set(task.setdefault("attributes", {}).get("release_artifacts", []))
    for upstream_artifact in worker["upstream-artifacts"]:
        for path in upstream_artifact["paths"]:
            artifacts.update(
                get_signed_artifacts(
                    input=path,
                    formats=upstream_artifact["formats"],
                )
            )
    task["attributes"]["release_artifacts"] = sorted(list(artifacts))
