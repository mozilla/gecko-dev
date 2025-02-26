# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import re
import shlex

from taskgraph.decision import PER_PROJECT_PARAMETERS
from taskgraph.parameters import extend_parameters_schema
from taskgraph.util.vcs import get_repository
from voluptuous import Any, Optional, Required


TRY_SYNTAX_RE = re.compile(r"\btry:\s*(.*)\s*$", re.M)

def parse_try_syntax(message):
    parser = argparse.ArgumentParser()
    parser.add_argument("--nspr-patch", action="store_true")
    parser.add_argument("-b", "--build", default="do")
    parser.add_argument("-p", "--platform", default="all")
    parser.add_argument("-u", "--unittests", default="none")
    parser.add_argument("-t", "--tools", default="none")
    parser.add_argument("-e", "--extra-builds")
    match = TRY_SYNTAX_RE.search(message)
    if not match:
        return
    args = shlex.split(match.group(1))
    opts = parser.parse_args(args)
    builds = [t for t in opts.build if t in ["d", "o"]]
    # If the given value is nonsense default to debug and opt builds.
    if not builds:
        builds = ["d", "o"]

    platforms = opts.platform.split(",")

    unittests = opts.unittests.split(",")
    if "gtests" in unittests:
        unittests.append("gtest")

    tools = opts.tools.split(",")
    return {
        "nspr_patch": opts.nspr_patch,
        "builds": builds,
        "platforms": platforms,
        "unittests": unittests,
        "tools": tools,
        "extra": opts.extra_builds == "all",
    }


def decision_parameters(graph_config, parameters):
    repo_path = os.getcwd()
    repo = get_repository(repo_path)
    try:
        commit_message = repo.get_commit_message()
    except UnicodeDecodeError:
        commit_message = ""
    parameters["try_options"] = {}
    if parameters["project"] != "nss-try":
        return
    args = parse_try_syntax(commit_message)
    if args:
       parameters["try_options"] = args


def default_parameters(repo_root):
    return {
        "try_options": {},
    }


def register(graph_config):
    schema = {
        Optional("try_options"): {
            Optional("nspr_patch"): bool,
            Optional("builds"): [Any("d", "o")],
            Optional("platforms"): [str],
            Optional("unittests"): [str],
            Optional("tools"): [str],
            Optional("extra"): bool,
        },
    }
    extend_parameters_schema(schema, default_parameters)

    from . import target_tasks

    PER_PROJECT_PARAMETERS["nss-try"] = {"target_tasks_method": "nss_try_tasks"}

