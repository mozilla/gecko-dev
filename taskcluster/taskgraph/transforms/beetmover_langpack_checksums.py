# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform release-beetmover-langpack-checksums into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.scriptworker import (get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         get_worker_type_for_scope)
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

taskref_or_string = Any(
    basestring,
    {Required('task-reference'): basestring})

beetmover_checksums_description_schema = schema.extend({
    Required('depname', default='build'): basestring,
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],
    Optional('locale'): basestring,
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
})

transforms = TransformSequence()
transforms.add_validate(beetmover_checksums_description_schema)


@transforms.add
def make_beetmover_checksums_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = dep_job.attributes

        treeherder = job.get('treeherder', {})
        treeherder.setdefault(
            'symbol',
            'BMcslang(N{})'.format(attributes.get('l10n_chunk', ''))
            )
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        label = job['label']
        build_platform = attributes.get('build_platform')

        description = "Beetmover submission of checksums for langpack files"

        extra = {}
        if 'devedition' in build_platform:
            extra['product'] = 'devedition'
        else:
            extra['product'] = 'firefox'

        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}
        for k, v in dep_job.dependencies.items():
            if k.startswith('beetmover'):
                dependencies[k] = v

        attributes = copy_attributes_from_dependent_job(dep_job)
        if 'chunk_locales' in dep_job.attributes:
            attributes['chunk_locales'] = dep_job.attributes['chunk_locales']

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, bucket_scope),
            'scopes': [bucket_scope, action_scope],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
            'extra': extra,
        }

        if 'shipping-phase' in job:
            task['shipping-phase'] = job['shipping-phase']

        if 'shipping-product' in job:
            task['shipping-product'] = job['shipping-product']

        yield task


def generate_upstream_artifacts(refs, platform, locales=None):
    # Until bug 1331141 is fixed, if you are adding any new artifacts here that
    # need to be transfered to S3, please be aware you also need to follow-up
    # with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
    # See example in bug 1348286
    common_paths = [
        "public/target-langpack.checksums",
    ]

    upstream_artifacts = []
    for locale in locales:
        upstream_artifacts.append({
            "taskId": {"task-reference": refs["beetmover"]},
            "taskType": "signing",
            "paths": common_paths,
            "locale": locale or "en-US",
        })

    return upstream_artifacts


@transforms.add
def make_beetmover_checksums_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = (len(job["dependencies"]) == 1)
        if not valid_beetmover_job:
            raise NotImplementedError("Beetmover checksums must have one dependency.")

        locales = job["attributes"].get("chunk_locales")
        platform = job["attributes"]["build_platform"]

        refs = {
            "beetmover": None,
        }
        for dependency in job["dependencies"].keys():
            if dependency.startswith("release-beetmover"):
                refs['beetmover'] = "<{}>".format(dependency)
        if None in refs.values():
            raise NotImplementedError(
                "Beetmover checksums must have a beetmover dependency!")

        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': generate_upstream_artifacts(
                refs, platform, locales
            ),
        }

        job["worker"] = worker

        yield job
