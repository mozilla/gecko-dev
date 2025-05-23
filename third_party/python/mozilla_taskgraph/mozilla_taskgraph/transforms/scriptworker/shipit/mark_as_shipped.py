# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from textwrap import dedent

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import Schema, optionally_keyed_by, resolve_keyed_by
from voluptuous import Extra, Optional

transforms = TransformSequence()

mark_as_shipped_schema = Schema(
    {
        Optional("name"): str,
        Optional("shipit-product"): optionally_keyed_by("build-type", str),
        Extra: object,
    }
)

transforms.add_validate(mark_as_shipped_schema)


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        resolve_keyed_by(
            task,
            "shipit-product",
            item_name=task.get("name", "mark-as-shipped"),
            **{
                "build-type": task.get("attributes", {}).get("build-type"),
            },
        )
        yield task


@transforms.add
def make_task_description(config, tasks):
    shipit = {}
    if "shipit" in config.graph_config:
        shipit = config.graph_config["shipit"]

    scriptworker = {}
    if "scriptworker" in config.graph_config:
        scriptworker = config.graph_config["scriptworker"]

    release_format = shipit.get(
        "release-format", "{product}-{version}-build{build_number}"
    )
    scope_prefix = shipit.get(
        "scope-prefix", f"{scriptworker.get('scope-prefix', 'project:releng')}:ship-it"
    )
    shipit_server = "production" if config.params["level"] == "3" else "staging"
    version = config.params.get("version", "<ver>")

    for task in tasks:
        product = task.pop("shipit-product", shipit.get("product"))

        if not product:
            config_path = os.path.join(config.graph_config.root_dir, "config.yml")
            raise Exception(
                dedent(
                    f"""
                Can't determine ShipIt product!

                Define it as `shipit-product` in the task definition, or as `shipit.product`
                in {config_path}.""".lstrip()
                )
            )

        task.setdefault("label", task.pop("name", "mark-as-shipped"))
        task["description"] = f"Mark {product} as shipped in Ship-It"
        task["scopes"] = [
            f"{scope_prefix}:action:mark-as-shipped",
            f"{scope_prefix}:server:{shipit_server}",
        ]
        task.setdefault("worker", {})["release-name"] = release_format.format(
            product=product,
            version=version,
            build_number=config.params.get("build_number", 1),
        )
        yield task
