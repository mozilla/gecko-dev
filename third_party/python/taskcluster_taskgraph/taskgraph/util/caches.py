# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import hashlib
from typing import TYPE_CHECKING, Any, Dict

if TYPE_CHECKING:
    from taskgraph.transforms.base import TransformConfig


def get_checkout_dir(task: Dict[str, Any]) -> str:
    worker = task["worker"]
    if worker["os"] == "windows":
        return "build"
    elif worker["implementation"] == "docker-worker":
        return f"{task['run']['workdir']}/checkouts"
    else:
        return "checkouts"


def get_checkout_cache_name(config: "TransformConfig", task: Dict[str, Any]) -> str:
    repo_configs = config.repo_configs
    cache_name = "checkouts"

    # Robust checkout does not clean up subrepositories, so ensure  that tasks
    # that checkout different sets of paths have separate caches.
    # See https://bugzilla.mozilla.org/show_bug.cgi?id=1631610
    if len(repo_configs) > 1:
        checkout_paths = {
            "\t".join([repo_config.path, repo_config.prefix])
            for repo_config in sorted(
                repo_configs.values(), key=lambda repo_config: repo_config.path
            )
        }
        checkout_paths_str = "\n".join(checkout_paths).encode("utf-8")
        digest = hashlib.sha256(checkout_paths_str).hexdigest()
        cache_name += f"-repos-{digest}"

    # Sparse checkouts need their own cache because they can interfere
    # with clients that aren't sparse aware.
    if task["run"]["sparse-profile"]:
        cache_name += "-sparse"

    return cache_name


CACHES = {
    "cargo": {"env": "CARGO_HOME"},
    "checkout": {
        "cache_dir": get_checkout_dir,
        "cache_name": get_checkout_cache_name,
    },
    "npm": {"env": "npm_config_cache"},
    "pip": {"env": "PIP_CACHE_DIR"},
    "uv": {"env": "UV_CACHE_DIR"},
}
