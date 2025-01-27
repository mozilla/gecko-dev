# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Tests for the 'job' transform subsystem.
"""


import os
from copy import deepcopy

import pytest
from mozunit import main
from taskgraph.config import load_graph_config
from taskgraph.transforms.base import TransformConfig

from gecko_taskgraph import GECKO
from gecko_taskgraph.test.conftest import FakeParameters
from gecko_taskgraph.transforms import job
from gecko_taskgraph.transforms.job import run_task  # noqa: F401
from gecko_taskgraph.transforms.task import group_name_variant

here = os.path.abspath(os.path.dirname(__file__))


TASK_DEFAULTS = {
    "description": "fake description",
    "label": "fake-task-label",
    "run": {
        "using": "run-task",
    },
}


@pytest.fixture(scope="module")
def config():
    graph_config = load_graph_config(os.path.join(GECKO, "taskcluster"))
    params = FakeParameters(
        {
            "base_repository": "http://hg.example.com",
            "head_repository": "http://hg.example.com",
            "head_rev": "abcdef",
            "level": 1,
            "project": "example",
        }
    )
    return TransformConfig(
        "job_test", here, {}, params, {}, graph_config, write_artifacts=False
    )


@pytest.fixture()
def transform(monkeypatch, config):
    """Run the job transforms on the specified task but return the inputs to
    `configure_taskdesc_for_run` without executing it.

    This gives test functions an easy way to generate the inputs required for
    many of the `run_using` subsystems.
    """

    def inner(task_input):
        task = deepcopy(TASK_DEFAULTS)
        task.update(task_input)
        frozen_args = []

        def _configure_taskdesc_for_run(*args):
            frozen_args.extend(args)

        monkeypatch.setattr(
            job, "configure_taskdesc_for_run", _configure_taskdesc_for_run
        )

        for _ in job.transforms(config, [task]):
            # This forces the generator to be evaluated
            pass

        return frozen_args

    return inner


@pytest.mark.parametrize(
    "groupSymbol,description",
    [
        pytest.param("M", "Mochitests", id="no_variants"),
        pytest.param(
            "M-spi-nw",
            "Mochitests with networking on socket process enabled",
            id="spi-nw variant",
        ),
        pytest.param(
            "M-spi-nw-http3",
            "Mochitests with networking on socket process enabled with http3 server",
            id="spi-nw and http3 variants",
        ),
        pytest.param("M-fake", "", id="invalid group name"),
    ],
    ids=lambda t: t["worker-type"],
)
def test_group_name(config, groupSymbol, description):
    group_names = config.graph_config["treeherder"]["group-names"]
    generated_description = group_name_variant(group_names, groupSymbol)
    assert description == generated_description


if __name__ == "__main__":
    main()
