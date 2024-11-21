# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

__version__ = "12.1.0"

# Maximum number of dependencies a single task can have
# https://docs.taskcluster.net/docs/reference/platform/queue/api#createTask
# specifies 10000, but we also optionally add the decision task id as a dep in
# taskgraph.create, so let's set this to 9999.
MAX_DEPENDENCIES = 9999

# Enable fast task generation for local debugging
# This is normally switched on via the --fast/-F flag to `mach taskgraph`
# Currently this skips toolchain task optimizations and schema validation
fast = False
