# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.signed_artifacts import generate_specifications_of_artifacts_to_sign


transforms = TransformSequence()


@transforms.add
def add_signed_routes(config, jobs):
    """Add routes corresponding to the routes of the build task
       this corresponds to, with .signed inserted, for all gecko.v2 routes"""

    for job in jobs:
        dep_job = job['primary-dependency']

        job['routes'] = []
        if dep_job.attributes.get('shippable'):
            for dep_route in dep_job.task.get('routes', []):
                if not dep_route.startswith('index.gecko.v2'):
                    continue
                branch = dep_route.split(".")[3]
                rest = ".".join(dep_route.split(".")[4:])
                job['routes'].append(
                    'index.gecko.v2.{}.signed.{}'.format(branch, rest))
        if dep_job.attributes.get('nightly'):
            for dep_route in dep_job.task.get('routes', []):
                if not dep_route.startswith('index.gecko.v2'):
                    continue
                branch = dep_route.split(".")[3]
                rest = ".".join(dep_route.split(".")[4:])
                job['routes'].append(
                    'index.gecko.v2.{}.signed-nightly.{}'.format(branch, rest))

        yield job


@transforms.add
def define_upstream_artifacts(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']

        job['attributes'] = copy_attributes_from_dependent_job(dep_job)

        artifacts_specifications = generate_specifications_of_artifacts_to_sign(
            config,
            job,
            keep_locale_template=False,
            kind=config.kind,
        )

        job['upstream-artifacts'] = [{
            'taskId': {'task-reference': '<build>'},
            'taskType': 'build',
            'paths': spec['artifacts'],
            'formats': spec['formats'],
        } for spec in artifacts_specifications]

        yield job
