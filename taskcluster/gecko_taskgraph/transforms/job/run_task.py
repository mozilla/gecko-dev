# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running jobs that are invoked via the `run-task` script.
"""


import os

from mozbuild.util import memoize
from mozpack import path
from taskgraph.transforms.run.common import support_caches
from taskgraph.util.schema import Schema
from taskgraph.util.yaml import load_yaml
from voluptuous import Any, Optional, Required

from gecko_taskgraph.transforms.job import run_job_using
from gecko_taskgraph.transforms.job.common import add_tooltool, support_vcs_checkout
from gecko_taskgraph.transforms.task import taskref_or_string

run_task_schema = Schema(
    {
        Required("using"): "run-task",
        # Use the specified caches.
        Optional("use-caches"): Any(bool, [str]),
        # if true (the default), perform a checkout of gecko on the worker
        Required("checkout"): bool,
        Optional(
            "cwd",
            description="Path to run command in. If a checkout is present, the path "
            "to the checkout will be interpolated with the key `checkout`",
        ): str,
        # The sparse checkout profile to use. Value is the filename relative to
        # "sparse-profile-prefix" which defaults to "build/sparse-profiles/".
        Required("sparse-profile"): Any(str, None),
        # The relative path to the sparse profile.
        Optional("sparse-profile-prefix"): str,
        # if true, perform a checkout of a comm-central based branch inside the
        # gecko checkout
        Required("comm-checkout"): bool,
        # The command arguments to pass to the `run-task` script, after the
        # checkout arguments.  If a list, it will be passed directly; otherwise
        # it will be included in a single argument to `bash -cx`.
        Required("command"): Any([taskref_or_string], taskref_or_string),
        # Base work directory used to set up the task.
        Optional("workdir"): str,
        # If not false, tooltool downloads will be enabled via relengAPIProxy
        # for either just public files, or all files. Only supported on
        # docker-worker.
        Required("tooltool-downloads"): Any(
            False,
            "public",
            "internal",
        ),
        # Whether to run as root. (defaults to False)
        Optional("run-as-root"): bool,
    }
)


def common_setup(config, job, taskdesc, command):
    run = job["run"]
    run_cwd = run.get("cwd")
    if run["checkout"]:
        gecko_path = support_vcs_checkout(config, job, taskdesc)
        command.append("--gecko-checkout={}".format(gecko_path))

        if run_cwd:
            run_cwd = path.normpath(run_cwd.format(checkout=gecko_path))

    elif run_cwd and "{checkout}" in run_cwd:
        raise Exception(
            "Found `{{checkout}}` interpolation in `cwd` for task {name} "
            "but the task doesn't have a checkout: {cwd}".format(
                cwd=run_cwd, name=job.get("name", job.get("label"))
            )
        )

    if run["sparse-profile"]:
        sparse_profile_prefix = run.pop(
            "sparse-profile-prefix", "build/sparse-profiles"
        )
        sparse_profile_path = path.join(sparse_profile_prefix, run["sparse-profile"])
        command.append(f"--gecko-sparse-profile={sparse_profile_path}")

    if run_cwd:
        command.append(f"--task-cwd={run_cwd}")

    support_caches(config, job, taskdesc)
    taskdesc["worker"].setdefault("env", {})["MOZ_SCM_LEVEL"] = config.params["level"]


worker_defaults = {
    "checkout": True,
    "comm-checkout": False,
    "sparse-profile": None,
    "tooltool-downloads": False,
    "run-as-root": False,
}


load_yaml = memoize(load_yaml)


def script_url(config, script):
    if "MOZ_AUTOMATION" in os.environ and "TASK_ID" not in os.environ:
        raise Exception("TASK_ID must be defined to use run-task on generic-worker")
    task_id = os.environ.get("TASK_ID", "<TASK_ID>")
    tc_url = "http://firefox-ci-tc.services.mozilla.com"
    return f"{tc_url}/api/queue/v1/task/{task_id}/artifacts/public/{script}"


@run_job_using(
    "docker-worker", "run-task", schema=run_task_schema, defaults=worker_defaults
)
def docker_worker_run_task(config, job, taskdesc):
    run = job["run"]
    worker = taskdesc["worker"] = job["worker"]
    command = ["/builds/worker/bin/run-task"]
    common_setup(config, job, taskdesc, command)

    if run["tooltool-downloads"]:
        internal = run["tooltool-downloads"] == "internal"
        add_tooltool(config, job, taskdesc, internal=internal)

    run_command = run["command"]

    # dict is for the case of `{'task-reference': text_type}`.
    if isinstance(run_command, (str, dict)):
        run_command = ["bash", "-cx", run_command]
    if run["comm-checkout"]:
        command.append(
            "--comm-checkout={}/comm".format(taskdesc["worker"]["env"]["GECKO_PATH"])
        )
    if run["run-as-root"]:
        command.extend(("--user", "root", "--group", "root"))
    command.append("--")
    command.extend(run_command)
    worker["command"] = command


@run_job_using(
    "generic-worker", "run-task", schema=run_task_schema, defaults=worker_defaults
)
def generic_worker_run_task(config, job, taskdesc):
    run = job["run"]
    worker = taskdesc["worker"] = job["worker"]
    is_win = worker["os"] == "windows"
    is_mac = worker["os"] == "macosx"
    is_bitbar = worker["os"] == "linux-bitbar"
    is_lambda = worker["os"] == "linux-lambda"

    if run["tooltool-downloads"]:
        internal = run["tooltool-downloads"] == "internal"
        add_tooltool(config, job, taskdesc, internal=internal)

    if is_win:
        command = ["C:/mozilla-build/python3/python3.exe", "run-task"]
    elif is_mac:
        command = ["/usr/local/bin/python3", "run-task"]
    else:
        command = ["./run-task"]

    common_setup(config, job, taskdesc, command)

    worker.setdefault("mounts", [])
    worker["mounts"].append(
        {
            "content": {
                "url": script_url(config, "run-task"),
            },
            "file": "./run-task",
        }
    )
    if job.get("fetches", {}):
        worker["mounts"].append(
            {
                "content": {
                    "url": script_url(config, "fetch-content"),
                },
                "file": "./fetch-content",
            }
        )
    if run.get("checkout"):
        worker["mounts"].append(
            {
                "content": {
                    "url": script_url(config, "robustcheckout.py"),
                },
                "file": "./robustcheckout.py",
            }
        )

    run_command = run["command"]

    # dict is for the case of `{'task-reference': text_type}`.
    if isinstance(run_command, (str, dict)):
        if is_win:
            if isinstance(run_command, dict):
                for k in run_command.keys():
                    run_command[k] = f'"{run_command[k]}"'
            else:
                run_command = f'"{run_command}"'
        run_command = ["bash", "-cx", run_command]

    if run["comm-checkout"]:
        command.append(
            "--comm-checkout={}/comm".format(taskdesc["worker"]["env"]["GECKO_PATH"])
        )

    if run["run-as-root"]:
        command.extend(("--user", "root", "--group", "root"))
    command.append("--")
    if is_bitbar:
        # Use the bitbar wrapper script which sets up the device and adb
        # environment variables
        command.append("/builds/taskcluster/script.py")
    elif is_lambda:
        command.append("/home/ltuser/taskcluster/script.py")
    command.extend(run_command)

    if is_win:
        taskref = False
        for c in command:
            if isinstance(c, dict):
                taskref = True

        if taskref:
            cmd = []
            for c in command:
                if isinstance(c, dict):
                    for v in c.values():
                        cmd.append(v)
                else:
                    cmd.append(c)
            worker["command"] = [{"artifact-reference": " ".join(cmd)}]
        else:
            worker["command"] = [" ".join(command)]
    else:
        worker["command"] = [
            ["chmod", "+x", "run-task"],
            command,
        ]
