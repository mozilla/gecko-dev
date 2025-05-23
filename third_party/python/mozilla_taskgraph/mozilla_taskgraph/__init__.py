# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from importlib import import_module

from taskgraph.config import validate_graph_config
from taskgraph.util import schema

# Schemas for YAML files should use dashed identifiers by default. If there are
# components of the schema for which there is a good reason to use another format,
# exceptions can be added here.
schema.EXCEPTED_SCHEMA_IDENTIFIERS.extend(
    [
        "bitrise",
    ]
)


def register(graph_config):
    # Import modules to register decorated functions
    _import_modules(
        [
            "actions",
            "config",
            "worker_types",
        ]
    )
    validate_graph_config(graph_config._config)


def _import_modules(modules):
    for module in modules:
        import_module(f".{module}", package=__name__)
