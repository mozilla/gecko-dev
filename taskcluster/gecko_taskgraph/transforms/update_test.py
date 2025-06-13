# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the update-test suite to parametrize by locale, source version, machine
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.copy import deepcopy

transforms = TransformSequence()

DEFAULT_VERSIONS_BACK = 3

DOCKER_TO_WORKER = {
    "ubuntu2404-test": "t-linux-docker",
    "ubuntu1804-test": "t-linux-docker",
}
DOCKER_TO_PLATFORM = {
    "ubuntu2404-test": "linux2404-64",
    "ubuntu1804-test": "linux1804-64",
}

TOP_LOCALES = [
    "en-US",
    "zh-CN",
    "de",
    "fr",
    "it",
    "es-ES",
    "pt-BR",
    "ru",
    "pl",
    "en-GB",
]

BASE_TYPE_COMMAND = "./mach update-test"

UPDATE_ARTIFACT_NAME = "public/update-test"


@transforms.add
def set_task_configuration(config, tasks):
    for task in tasks:
        for os in task["os"]:
            this_task = deepcopy(task)
            if os in DOCKER_TO_WORKER:
                worker_type = DOCKER_TO_WORKER[os]
                platform = DOCKER_TO_PLATFORM.get(os)
                this_task["worker"]["docker-image"] = {}
                this_task["worker"]["docker-image"]["in-tree"] = os
            else:
                worker_type = os
                platform = worker_type

            this_task["name"] = f"{platform}-firefox"
            this_task["description"] = f"Test updates on {platform}"
            this_task["worker-type"] = worker_type
            this_task["treeherder"]["platform"] = f"{platform}/opt"
            this_task["index"]["job-name"] = f"update-test-{platform}"
            this_task["run"]["cwd"] = "{checkout}"
            del this_task["os"]

            yield this_task


def get_command_prefix(command):
    command_prefix = ""
    if "&&" in command:
        command_prefix, _ = command.rsplit("&& ", 1)
        command_prefix = command_prefix + "&&"
    return command_prefix


def infix_treeherder_symbol(symbol, infix):
    head, tail = symbol.split("(", 1)
    return f"{head}({tail[:-1]}-{infix})"


@transforms.add
def parametrize_by_locale_and_source_version(config, tasks):
    for task in tasks:
        command_prefix = get_command_prefix(task["run"]["command"])
        for locale in TOP_LOCALES:
            this_task = deepcopy(task)
            this_task["run"]["command"] = (
                f"{command_prefix}{BASE_TYPE_COMMAND} --source-locale {locale} "
                + f"--source-versions-back {DEFAULT_VERSIONS_BACK};"
            )
            this_task["description"] = (
                f'{this_task["description"]}, locale coverage: {locale}'
            )
            this_task["name"] = f'{this_task["name"]}-locale-{locale}'
            this_task["index"][
                "job-name"
            ] = f'{this_task["index"]["job-name"]}-locale-{locale}"'
            this_task["treeherder"]["symbol"] = infix_treeherder_symbol(
                this_task["treeherder"]["symbol"], locale
            )
            yield this_task

        # NB: We actually want source_versions_back = 0, because it gives us oldest usable ver
        for v in range(5):
            # avoid tasks with different names, same defs
            if v == DEFAULT_VERSIONS_BACK:
                continue
            this_task = deepcopy(task)
            this_task["run"][
                "command"
            ] = f"{command_prefix}{BASE_TYPE_COMMAND} --source-versions-back {v};"
            description_tag = (
                " from 3 major versions back" if v == 0 else f" from {v} releases back"
            )
            this_task["description"] = this_task["description"] + description_tag
            ago_tag = "-from-oldest" if v == 0 else f"-from-{v}-ago"
            this_task["name"] = this_task["name"] + ago_tag
            this_task["index"]["job-name"] = this_task["index"]["job-name"] + ago_tag
            this_task["treeherder"]["symbol"] = infix_treeherder_symbol(
                this_task["treeherder"]["symbol"], ago_tag.split("-", 2)[-1]
            )
            yield this_task

        # default task is actually a background update
        task["name"] = task["name"] + "-bkg"
        task["index"]["job-name"] = task["index"]["job-name"] + "-bkg"
        task["treeherder"]["symbol"] = infix_treeherder_symbol(
            task["treeherder"]["symbol"], "bkg"
        )
        yield task
