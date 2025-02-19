# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Common support for various task types.  These functions are all named after the
worker implementation they operate on, and take the same three parameters, for
consistency.
"""

import json
from typing import Any, Dict, List, Union

from taskgraph.transforms.base import TransformConfig
from taskgraph.util import path
from taskgraph.util.caches import CACHES, get_checkout_dir
from taskgraph.util.taskcluster import get_artifact_prefix


def get_vcsdir_name(os):
    if os == "windows":
        return "src"
    else:
        return "vcs"


def add_cache(task, taskdesc, name, mount_point, skip_untrusted=False):
    """Adds a cache based on the worker's implementation.

    Args:
        task (dict): Tasks object.
        taskdesc (dict): Target task description to modify.
        name (str): Name of the cache.
        mount_point (path): Path on the host to mount the cache.
        skip_untrusted (bool): Whether cache is used in untrusted environments
            (default: False). Only applies to docker-worker.
    """
    worker = task["worker"]
    if worker["implementation"] not in ("docker-worker", "generic-worker"):
        # caches support not implemented
        return

    if worker["implementation"] == "docker-worker":
        taskdesc["worker"].setdefault("caches", []).append(
            {
                "type": "persistent",
                "name": name,
                "mount-point": mount_point,
                "skip-untrusted": skip_untrusted,
            }
        )

    elif worker["implementation"] == "generic-worker":
        taskdesc["worker"].setdefault("mounts", []).append(
            {
                "cache-name": name,
                "directory": mount_point,
            }
        )


def add_artifacts(config, task, taskdesc, path):
    taskdesc["worker"].setdefault("artifacts", []).append(
        {
            "name": get_artifact_prefix(taskdesc),
            "path": path,
            "type": "directory",
        }
    )


def docker_worker_add_artifacts(config, task, taskdesc):
    """Adds an artifact directory to the task"""
    path = "{workdir}/artifacts/".format(**task["run"])
    taskdesc["worker"]["env"]["UPLOAD_DIR"] = path
    add_artifacts(config, task, taskdesc, path)


def generic_worker_add_artifacts(config, task, taskdesc):
    """Adds an artifact directory to the task"""
    # The path is the location on disk; it doesn't necessarily
    # mean the artifacts will be public or private; that is set via the name
    # attribute in add_artifacts.
    add_artifacts(config, task, taskdesc, path=get_artifact_prefix(taskdesc))


def support_vcs_checkout(config, task, taskdesc, repo_configs, sparse=False):
    """Update a task with parameters to enable a VCS checkout.

    This can only be used with ``run-task`` tasks, as the cache name is
    reserved for ``run-task`` tasks.
    """
    worker = task["worker"]
    assert worker["os"] in ("linux", "macosx", "windows")
    is_win = worker["os"] == "windows"
    is_docker = worker["implementation"] == "docker-worker"

    checkoutdir = get_checkout_dir(task)
    if is_win:
        hgstore = "y:/hg-shared"
    elif is_docker:
        hgstore = f"{checkoutdir}/hg-store"
    else:
        hgstore = f"{checkoutdir}/hg-shared"

    vcsdir = f"{checkoutdir}/{get_vcsdir_name(worker['os'])}"
    env = taskdesc["worker"].setdefault("env", {})
    env.update(
        {
            "HG_STORE_PATH": hgstore,
            "REPOSITORIES": json.dumps(
                {repo.prefix: repo.name for repo in repo_configs.values()}
            ),
            "VCS_PATH": vcsdir,
        }
    )
    for repo_config in repo_configs.values():
        env.update(
            {
                f"{repo_config.prefix.upper()}_{key}": value
                for key, value in {
                    "BASE_REPOSITORY": repo_config.base_repository,
                    "HEAD_REPOSITORY": repo_config.head_repository,
                    "HEAD_REV": repo_config.head_rev,
                    "HEAD_REF": repo_config.head_ref,
                    "REPOSITORY_TYPE": repo_config.type,
                    "SSH_SECRET_NAME": repo_config.ssh_secret_name,
                }.items()
                if value is not None
            }
        )
        if repo_config.ssh_secret_name:
            taskdesc["scopes"].append(f"secrets:get:{repo_config.ssh_secret_name}")

    # only some worker platforms have taskcluster-proxy enabled
    if task["worker"]["implementation"] in ("docker-worker",):
        taskdesc["worker"]["taskcluster-proxy"] = True

    return vcsdir


def should_use_cache(
    name: str,
    use_caches: Union[bool, List[str]],
    has_checkout: bool,
) -> bool:
    # Never enable the checkout cache if there's no clone. This allows
    # 'checkout' to be specified as a default cache without impacting
    # irrelevant tasks.
    if name == "checkout" and not has_checkout:
        return False

    if isinstance(use_caches, bool):
        return use_caches

    return name in use_caches


def support_caches(
    config: TransformConfig, task: Dict[str, Any], taskdesc: Dict[str, Any]
):
    """Add caches for common tools."""
    run = task["run"]
    worker = task["worker"]
    workdir = run.get("workdir")
    base_cache_dir = ".task-cache"
    if worker["implementation"] == "docker-worker":
        workdir = workdir or "/builds/worker"
        base_cache_dir = path.join(workdir, base_cache_dir)

    use_caches = run.get("use-caches")
    if use_caches is None:
        # Use project default values for filtering caches, default to
        # checkout cache if no selection is specified.
        use_caches = (
            config.graph_config.get("taskgraph", {})
            .get("run", {})
            .get("use-caches", ["checkout"])
        )

    for name, cache_cfg in CACHES.items():
        if not should_use_cache(name, use_caches, run["checkout"]):
            continue

        if "cache_dir" in cache_cfg:
            assert callable(cache_cfg["cache_dir"])
            cache_dir = cache_cfg["cache_dir"](task)
        else:
            cache_dir = f"{base_cache_dir}/{name}"

        if "cache_name" in cache_cfg:
            assert callable(cache_cfg["cache_name"])
            cache_name = cache_cfg["cache_name"](config, task)
        else:
            cache_name = name

        if cache_cfg.get("env"):
            env = taskdesc["worker"].setdefault("env", {})
            # If cache_dir is already absolute, the `.join` call returns it as
            # is. In that case, {task_workdir} will get interpolated by
            # run-task.
            env[cache_cfg["env"]] = cache_dir
        add_cache(task, taskdesc, cache_name, cache_dir)
