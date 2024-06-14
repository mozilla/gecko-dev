# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.util.attributes import release_level

logger = logging.getLogger(__name__)


transforms = TransformSequence()


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job,
            "scopes",
            item_name=job["name"],
            **{"release-level": release_level(config.params["project"])}
        )
        job["worker"]["product"] = job["shipping-product"]
        job["worker"]["version"] = config.params["version"]
        job["worker"]["channel"] = config.params["release_type"]
        yield job
