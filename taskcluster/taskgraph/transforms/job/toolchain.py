# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running toolchain-building jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

from mozbuild.shellutil import quote as shell_quote

from taskgraph.util.schema import Schema
from voluptuous import Optional, Required, Any

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_artifacts,
    docker_worker_add_tooltool,
    generic_worker_hg_commands,
    support_vcs_checkout,
)
from taskgraph.util.hash import hash_paths
from taskgraph import GECKO
from taskgraph.util.cached_tasks import add_optimization
import taskgraph


CACHE_TYPE = 'toolchains.v2'

toolchain_run_schema = Schema({
    Required('using'): 'toolchain-script',

    # The script (in taskcluster/scripts/misc) to run.
    # Python scripts are invoked with `mach python` so vendored libraries
    # are available.
    Required('script'): basestring,

    # Arguments to pass to the script.
    Optional('arguments'): [basestring],

    # If not false, tooltool downloads will be enabled via relengAPIProxy
    # for either just public files, or all files.  Not supported on Windows
    Required('tooltool-downloads'): Any(
        False,
        'public',
        'internal',
    ),

    # Sparse profile to give to checkout using `run-task`.  If given,
    # a filename in `build/sparse-profiles`.  Defaults to
    # "toolchain-build", i.e., to
    # `build/sparse-profiles/toolchain-build`.  If `None`, instructs
    # `run-task` to not use a sparse profile at all.
    Required('sparse-profile'): Any(basestring, None),

    # Paths/patterns pointing to files that influence the outcome of a
    # toolchain build.
    Optional('resources'): [basestring],

    # Path to the artifact produced by the toolchain job
    Required('toolchain-artifact'): basestring,

    # An alias that can be used instead of the real toolchain job name in
    # the toolchains list for build jobs.
    Optional('toolchain-alias'): basestring,

    # Base work directory used to set up the task.
    Required('workdir'): basestring,
})


def get_digest_data(config, run, taskdesc):
    files = list(run.get('resources', []))
    # This file
    files.append('taskcluster/taskgraph/transforms/job/toolchain.py')
    # The script
    files.append('taskcluster/scripts/misc/{}'.format(run['script']))
    # Tooltool manifest if any is defined:
    tooltool_manifest = taskdesc['worker']['env'].get('TOOLTOOL_MANIFEST')
    if tooltool_manifest:
        files.append(tooltool_manifest)

    # Accumulate dependency hashes for index generation.
    data = [hash_paths(GECKO, files)]

    # If the task has dependencies, we need those dependencies to influence
    # the index path. So take the digest from the files above, add the list
    # of its dependencies, and hash the aggregate.
    # If the task has no dependencies, just use the digest from above.
    deps = taskdesc['dependencies']
    if deps:
        data.extend(sorted(deps.values()))

    # If the task uses an in-tree docker image, we want it to influence
    # the index path as well. Ideally, the content of the docker image itself
    # should have an influence, but at the moment, we can't get that
    # information here. So use the docker image name as a proxy. Not a lot of
    # changes to docker images actually have an impact on the resulting
    # toolchain artifact, so we'll just rely on such important changes to be
    # accompanied with a docker image name change.
    image = taskdesc['worker'].get('docker-image', {}).get('in-tree')
    if image:
        data.extend(image)

    # Likewise script arguments should influence the index.
    args = run.get('arguments')
    if args:
        data.extend(args)
    return data


toolchain_defaults = {
    'tooltool-downloads': False,
    'sparse-profile': 'toolchain-build',
}


@run_job_using("docker-worker", "toolchain-script",
               schema=toolchain_run_schema, defaults=toolchain_defaults)
def docker_worker_toolchain(config, job, taskdesc):
    run = job['run']

    worker = taskdesc['worker']
    worker['chain-of-trust'] = True

    # Allow the job to specify where artifacts come from, but add
    # public/build if it's not there already.
    artifacts = worker.setdefault('artifacts', [])
    if not any(artifact.get('name') == 'public/build' for artifact in artifacts):
        docker_worker_add_artifacts(config, job, taskdesc)

    support_vcs_checkout(config, job, taskdesc, sparse=True)

    # Toolchain checkouts don't live under {workdir}/checkouts
    workspace = '{workdir}/workspace/build'.format(**run)
    gecko_path = '{}/src'.format(workspace)

    env = worker['env']
    env.update({
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'TOOLS_DISABLE': 'true',
        'MOZ_AUTOMATION': '1',
        'MOZ_FETCHES_DIR': workspace,
        'GECKO_PATH': gecko_path,
    })

    if run['tooltool-downloads']:
        internal = run['tooltool-downloads'] == 'internal'
        docker_worker_add_tooltool(config, job, taskdesc, internal=internal)

    # Use `mach` to invoke python scripts so in-tree libraries are available.
    if run['script'].endswith('.py'):
        wrapper = '{}/mach python '.format(gecko_path)
    else:
        wrapper = ''

    args = run.get('arguments', '')
    if args:
        args = ' ' + shell_quote(*args)

    sparse_profile = []
    if run.get('sparse-profile'):
        sparse_profile = ['--sparse-profile=build/sparse-profiles/{}'
                          .format(run['sparse-profile'])]

    worker['command'] = [
        '{workdir}/bin/run-task'.format(**run),
        '--vcs-checkout={}'.format(gecko_path),
    ] + sparse_profile + [
        '--',
        'bash',
        '-c',
        'cd {} && '
        '{}workspace/build/src/taskcluster/scripts/misc/{}{}'.format(
            run['workdir'], wrapper, run['script'], args)
    ]

    attributes = taskdesc.setdefault('attributes', {})
    attributes['toolchain-artifact'] = run['toolchain-artifact']
    if 'toolchain-alias' in run:
        attributes['toolchain-alias'] = run['toolchain-alias']

    if not taskgraph.fast:
        name = taskdesc['label'].replace('{}-'.format(config.kind), '', 1)
        add_optimization(
            config, taskdesc,
            cache_type=CACHE_TYPE,
            cache_name=name,
            digest_data=get_digest_data(config, run, taskdesc),
        )


@run_job_using("generic-worker", "toolchain-script",
               schema=toolchain_run_schema, defaults=toolchain_defaults)
def windows_toolchain(config, job, taskdesc):
    run = job['run']

    worker = taskdesc['worker']

    worker['artifacts'] = [{
        'path': r'public\build',
        'type': 'directory',
    }]
    worker['chain-of-trust'] = True

    support_vcs_checkout(config, job, taskdesc)

    env = worker['env']
    env.update({
        'MOZ_BUILD_DATE': config.params['moz_build_date'],
        'MOZ_SCM_LEVEL': config.params['level'],
        'MOZ_AUTOMATION': '1',
    })

    hg_command = generic_worker_hg_commands(
        'https://hg.mozilla.org/mozilla-unified',
        env['GECKO_HEAD_REPOSITORY'],
        env['GECKO_HEAD_REV'],
        r'.\build\src')[0]

    # Use `mach` to invoke python scripts so in-tree libraries are available.
    if run['script'].endswith('.py'):
        raise NotImplementedError("Python scripts don't work on Windows")

    args = run.get('arguments', '')
    if args:
        args = ' ' + shell_quote(*args)

    bash = r'c:\mozilla-build\msys\bin\bash'
    worker['command'] = [
        hg_command,
        # do something intelligent.
        r'{} build/src/taskcluster/scripts/misc/{}{}'.format(
            bash, run['script'], args)
    ]

    attributes = taskdesc.setdefault('attributes', {})
    attributes['toolchain-artifact'] = run['toolchain-artifact']
    if 'toolchain-alias' in run:
        attributes['toolchain-alias'] = run['toolchain-alias']

    if not taskgraph.fast:
        name = taskdesc['label'].replace('{}-'.format(config.kind), '', 1)
        add_optimization(
            config, taskdesc,
            cache_type=CACHE_TYPE,
            cache_name=name,
            digest_data=get_digest_data(config, run, taskdesc),
        )
