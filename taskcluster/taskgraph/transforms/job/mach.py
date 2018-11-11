# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running mach tasks (via run-task)
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.run_task import docker_worker_run_task
from voluptuous import Schema, Required

mach_schema = Schema({
    Required('using'): 'mach',

    # The mach command (omitting `./mach`) to run
    Required('mach'): basestring,
})


@run_job_using("docker-worker", "mach", schema=mach_schema)
def docker_worker_mach(config, job, taskdesc):
    run = job['run']

    # defer to the run_task implementation
    run['command'] = 'cd /home/worker/checkouts/gecko && ./mach ' + run['mach']
    run['checkout'] = True
    del run['mach']
    docker_worker_run_task(config, job, taskdesc)
