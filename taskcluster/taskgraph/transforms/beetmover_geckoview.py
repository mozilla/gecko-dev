# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import \
    craft_release_properties as beetmover_craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import resolve_keyed_by, optionally_keyed_by
from taskgraph.util.scriptworker import get_worker_type_for_scope
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional


_ARTIFACT_ID_PER_PLATFORM = {
    'android-aarch64': 'geckoview{update_channel}-arm64-v8a',
    'android-api-16': 'geckoview{update_channel}-armeabi-v7a',
    'android-x86': 'geckoview{update_channel}-x86',
    'android-x86_64': 'geckoview{update_channel}-x86_64',
}

_MOZ_UPDATE_CHANNEL_PER_BRANCH = {
    'mozilla-release': '',
    'mozilla-beta': '-beta',
    'mozilla-central': '-nightly',
    'try': '-nightly-try',
    'maple': '-nightly-maple',
}

task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

beetmover_description_schema = schema.extend({
    Required('depname', default='build'): basestring,
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],

    Required('run-on-projects'): task_description_schema['run-on-projects'],
    Required('run-on-hg-branches'): task_description_schema['run-on-hg-branches'],

    Optional('bucket-scope'): optionally_keyed_by('release-level', basestring),
    Optional('shipping-phase'): optionally_keyed_by(
        'project', task_description_schema['shipping-phase']
    ),
    Optional('shipping-product'): task_description_schema['shipping-product'],
})

transforms = TransformSequence()
transforms.add_validate(beetmover_description_schema)


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'run-on-hg-branches', item_name=job['label'], project=config.params['project']
        )
        resolve_keyed_by(
            job, 'shipping-phase', item_name=job['label'], project=config.params['project']
        )
        resolve_keyed_by(
            job, 'bucket-scope', item_name=job['label'],
            **{'release-level': config.params.release_level()}
        )
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = dep_job.attributes

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'BM-gv')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              '{}/opt'.format(dep_th_platform))
        treeherder.setdefault('tier', 2)
        treeherder.setdefault('kind', 'build')
        label = job['label']
        description = (
            "Beetmover submission for geckoview"
            "{build_platform}/{build_type}'".format(
                build_platform=attributes.get('build_platform'),
                build_type=attributes.get('build_type')
            )
        )

        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}

        attributes = copy_attributes_from_dependent_job(dep_job)

        if job.get('locale'):
            attributes['locale'] = job['locale']

        attributes['run_on_hg_branches'] = job['run-on-hg-branches']

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, job['bucket-scope']),
            'scopes': [job['bucket-scope'], 'project:releng:beetmover:action:push-to-maven'],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': job['run-on-projects'],
            'treeherder': treeherder,
            'shipping-phase': job['shipping-phase'],
        }

        yield task


def generate_upstream_artifacts(build_task_ref):
    return [{
        'taskId': {'task-reference': build_task_ref},
        'taskType': 'build',
        'paths': ['public/build/target.maven.zip'],
        'zipExtract': True,
    }]


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = len(job['dependencies']) == 1 and 'build' in job['dependencies']
        if not valid_beetmover_job:
            raise NotImplementedError(
                'Beetmover-geckoview must have a single dependency. Got: {}'.format(
                    job['dependencies']
                )
            )

        build_task = list(job["dependencies"].keys())[0]
        build_task_ref = "<" + str(build_task) + ">"

        worker = {
            'implementation': 'beetmover-maven',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': generate_upstream_artifacts(build_task_ref)
        }

        job["worker"] = worker

        yield job


def craft_release_properties(config, job):
    props = beetmover_craft_release_properties(config, job)

    platform = props['platform']
    update_channel = _MOZ_UPDATE_CHANNEL_PER_BRANCH.get(
        props['branch'], '-UNKNOWN_MOZ_UPDATE_CHANNEL'
    )
    artifact_id = _ARTIFACT_ID_PER_PLATFORM[platform].format(update_channel=update_channel)
    props['artifact-id'] = artifact_id
    props['app-name'] = 'geckoview'     # this beetmover job is not about pushing Fennec

    return props
