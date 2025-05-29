# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import android_taskgraph
from taskgraph import config as taskgraph_config
from taskgraph import generator
from taskgraph import morph as taskgraph_morph
from taskgraph.transforms.task import payload_builders
from taskgraph.util import schema
from taskgraph.util import taskcluster as tc_util

from gecko_taskgraph.config import graph_config_schema

GECKO = os.path.normpath(os.path.realpath(os.path.join(__file__, "..", "..", "..")))

# Overwrite Taskgraph's default graph_config_schema with a custom one.
taskgraph_config.graph_config_schema = graph_config_schema

# Don't use any of the upstream morphs.
# TODO Investigate merging our morphs with upstream.
taskgraph_morph.registered_morphs = []

# Default rootUrl to use if none is given in the environment; this should point
# to the production Taskcluster deployment used for CI.
tc_util.PRODUCTION_TASKCLUSTER_ROOT_URL = "https://firefox-ci-tc.services.mozilla.com"

# Schemas for YAML files should use dashed identifiers by default. If there are
# components of the schema for which there is a good reason to use another format,
# exceptions can be added here.
schema.EXCEPTED_SCHEMA_IDENTIFIERS.extend(
    [
        "test_name",
        "json_location",
        "video_location",
        "profile_name",
        "target_path",
        "try_task_config",
    ]
)

# TODO: These are temporarily redefined in gecko_taskgraph. Remove them from
# upstream until they can be consolidated.
del payload_builders["beetmover"]
del payload_builders["docker-worker"]
del payload_builders["generic-worker"]


def register(graph_config):
    """Used to register Gecko specific extensions.

    Args:
        graph_config: The graph configuration object.
    """
    from taskgraph.optimize.base import registry

    from gecko_taskgraph import (  # noqa
        filter_tasks,
        morph,
        target_tasks,  # trigger target task method registration
    )
    from gecko_taskgraph.parameters import register_parameters
    from gecko_taskgraph.util import (
        dependencies,  # noqa - trigger group_by registration
    )
    from gecko_taskgraph.util.verify import verifications

    # TODO: Remove along with
    # `gecko_taskgraph.optimize.strategies.SkipUnlessChanged`
    # (see comment over there)
    del registry["skip-unless-changed"]

    android_taskgraph.register(graph_config)

    # Don't use the upstream verifications, and replace them with our own.
    # TODO Investigate merging our verifications with upstream.
    generator.verifications = verifications

    register_parameters()
