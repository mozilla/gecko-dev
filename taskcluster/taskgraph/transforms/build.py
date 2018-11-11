# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import RELEASE_PROJECTS
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.workertypes import worker_type_implementation

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def set_defaults(config, jobs):
    """Set defaults, including those that differ per worker implementation"""
    for job in jobs:
        job['treeherder'].setdefault('kind', 'build')
        job['treeherder'].setdefault('tier', 1)
        _, worker_os = worker_type_implementation(job['worker-type'])
        worker = job.setdefault('worker', {})
        worker.setdefault('env', {})
        if worker_os == "linux":
            worker.setdefault('docker-image', {'in-tree': 'debian7-amd64-build'})
            worker['chain-of-trust'] = True
        elif worker_os == "windows":
            worker['chain-of-trust'] = True

        yield job


@transforms.add
def stub_installer(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'stub-installer', item_name=job['name'], project=config.params['project']
        )
        job.setdefault('attributes', {})
        if job.get('stub-installer'):
            job['attributes']['stub-installer'] = job['stub-installer']
            job['worker']['env'].update({"USE_STUB_INSTALLER": "1"})
        if 'stub-installer' in job:
            del job['stub-installer']
        yield job


@transforms.add
def update_channel(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'run.update-channel', item_name=job['name'],
            **{
                'release-type': config.params['release_type'],
            }
        )
        update_channel = job['run'].pop('update-channel', None)
        if update_channel:
            job['run'].setdefault('extra-config', {})['update_channel'] = update_channel
        yield job


@transforms.add
def mozconfig(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'run.mozconfig-variant', item_name=job['name'],
            **{
                'release-type': config.params['release_type'],
            }
        )
        mozconfig_variant = job['run'].pop('mozconfig-variant', None)
        if mozconfig_variant:
            job['run'].setdefault('extra-config', {})['mozconfig_variant'] = mozconfig_variant
        yield job


@transforms.add
def set_env(config, jobs):
    """Set extra environment variables from try command line."""
    env = []
    if config.params['try_mode'] == 'try_option_syntax':
        env = config.params['try_options']['env'] or []
    for job in jobs:
        if env:
            job['worker']['env'].update(dict(x.split('=') for x in env))
        yield job


@transforms.add
def enable_full_crashsymbols(config, jobs):
    """Enable full crashsymbols on jobs with
    'enable-full-crashsymbols' set to True and on release branches, or
    on try"""
    branches = RELEASE_PROJECTS | {'try', }
    for job in jobs:
        enable_full_crashsymbols = job['attributes'].get('enable-full-crashsymbols')
        if enable_full_crashsymbols and config.params['project'] in branches:
            logger.debug("Enabling full symbol generation for %s", job['name'])
        else:
            logger.debug("Disabling full symbol generation for %s", job['name'])
            job['worker']['env']['MOZ_DISABLE_FULL_SYMBOLS'] = '1'
        yield job
