# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from voluptuous import Required
from taskgraph.util.taskcluster import get_artifact_url
from taskgraph.transforms.job import run_job_using
from taskgraph.util.schema import Schema
from taskgraph.util.taskcluster import get_artifact_path
from taskgraph.transforms.tests import (
    test_description_schema,
    normpath
)
from taskgraph.transforms.job.common import (
    docker_worker_add_tooltool,
    support_vcs_checkout,
)
import os

VARIANTS = [
    'nightly',
    'devedition',
    'pgo',
    'asan',
    'stylo',
    'stylo-disabled',
    'stylo-sequential',
    'qr',
    'ccov',
]


def get_variant(test_platform):
    for v in VARIANTS:
        if '-{}/'.format(v) in test_platform:
            return v
    return ''


test_description_schema = {str(k): v for k, v in test_description_schema.schema.iteritems()}

mozharness_test_run_schema = Schema({
    Required('using'): 'mozharness-test',
    Required('test'): test_description_schema,
    # Base work directory used to set up the task.
    Required('workdir'): basestring,
})


def test_packages_url(taskdesc):
    """Account for different platforms that name their test packages differently"""
    return get_artifact_url('<build>', get_artifact_path(taskdesc, 'target.test_packages.json'))


@run_job_using('docker-engine', 'mozharness-test', schema=mozharness_test_run_schema)
@run_job_using('docker-worker', 'mozharness-test', schema=mozharness_test_run_schema)
def mozharness_test_on_docker(config, job, taskdesc):
    run = job['run']
    test = taskdesc['run']['test']
    mozharness = test['mozharness']
    worker = taskdesc['worker']

    # apply some defaults
    worker['docker-image'] = test['docker-image']
    worker['allow-ptrace'] = True  # required for all tests, for crashreporter
    worker['loopback-video'] = test['loopback-video']
    worker['loopback-audio'] = test['loopback-audio']
    worker['max-run-time'] = test['max-run-time']
    worker['retry-exit-status'] = test['retry-exit-status']
    if 'android-em-7.0-x86' in test['test-platform']:
        worker['privileged'] = True

    artifacts = [
        # (artifact name prefix, in-image path)
        ("public/logs/", "{workdir}/workspace/logs/".format(**run)),
        ("public/test", "{workdir}/artifacts/".format(**run)),
        ("public/test_info/", "{workdir}/workspace/build/blobber_upload_dir/".format(**run)),
    ]

    installer_url = get_artifact_url('<build>', mozharness['build-artifact-name'])
    mozharness_url = get_artifact_url('<build>',
                                      get_artifact_path(taskdesc, 'mozharness.zip'))

    worker['artifacts'] = [{
        'name': prefix,
        'path': os.path.join('{workdir}/workspace'.format(**run), path),
        'type': 'directory',
    } for (prefix, path) in artifacts]

    worker['caches'] = [{
        'type': 'persistent',
        'name': 'level-{}-{}-test-workspace'.format(
            config.params['level'], config.params['project']),
        'mount-point': "{workdir}/workspace".format(**run),
    }]

    env = worker.setdefault('env', {})
    env.update({
        'MOZHARNESS_CONFIG': ' '.join(mozharness['config']),
        'MOZHARNESS_SCRIPT': mozharness['script'],
        'MOZILLA_BUILD_URL': {'task-reference': installer_url},
        'NEED_PULSEAUDIO': 'true',
        'NEED_WINDOW_MANAGER': 'true',
        'ENABLE_E10S': str(bool(test.get('e10s'))).lower(),
        'MOZ_AUTOMATION': '1',
        'WORKING_DIR': '/builds/worker',
    })

    if mozharness.get('mochitest-flavor'):
        env['MOCHITEST_FLAVOR'] = mozharness['mochitest-flavor']

    if mozharness['set-moz-node-path']:
        env['MOZ_NODE_PATH'] = '/usr/local/bin/node'

    if 'actions' in mozharness:
        env['MOZHARNESS_ACTIONS'] = ' '.join(mozharness['actions'])

    if config.params.is_try():
        env['TRY_COMMIT_MSG'] = config.params['message']

    # handle some of the mozharness-specific options

    if mozharness['tooltool-downloads']:
        docker_worker_add_tooltool(config, job, taskdesc, internal=True)

    if test['reboot']:
        raise Exception('reboot: {} not supported on generic-worker'.format(test['reboot']))

    # assemble the command line
    command = [
        '{workdir}/bin/run-task'.format(**run),
    ]

    # Support vcs checkouts regardless of whether the task runs from
    # source or not in case it is needed on an interactive loaner.
    support_vcs_checkout(config, job, taskdesc)

    # If we have a source checkout, run mozharness from it instead of
    # downloading a zip file with the same content.
    if test['checkout']:
        command.extend(['--vcs-checkout', '{workdir}/checkouts/gecko'.format(**run)])
        env['MOZHARNESS_PATH'] = '{workdir}/checkouts/gecko/testing/mozharness'.format(**run)
    else:
        env['MOZHARNESS_URL'] = {'task-reference': mozharness_url}

    command.extend([
        '--',
        '{workdir}/bin/test-linux.sh'.format(**run),
    ])

    command.extend([
        {"task-reference": "--installer-url=" + installer_url},
        {"task-reference": "--test-packages-url=" + test_packages_url(taskdesc)},
    ])
    command.extend(mozharness.get('extra-options', []))

    # TODO: remove the need for run['chunked']
    if mozharness.get('chunked') or test['chunks'] > 1:
        # Implement mozharness['chunking-args'], modifying command in place
        if mozharness['chunking-args'] == 'this-chunk':
            command.append('--total-chunk={}'.format(test['chunks']))
            command.append('--this-chunk={}'.format(test['this-chunk']))
        elif mozharness['chunking-args'] == 'test-suite-suffix':
            suffix = mozharness['chunk-suffix'].replace('<CHUNK>', str(test['this-chunk']))
            for i, c in enumerate(command):
                if isinstance(c, basestring) and c.startswith('--test-suite'):
                    command[i] += suffix

    if 'download-symbols' in mozharness:
        download_symbols = mozharness['download-symbols']
        download_symbols = {True: 'true', False: 'false'}.get(download_symbols, download_symbols)
        command.append('--download-symbols=' + download_symbols)

    worker['command'] = command


@run_job_using('generic-worker', 'mozharness-test', schema=mozharness_test_run_schema)
def mozharness_test_on_generic_worker(config, job, taskdesc):
    test = taskdesc['run']['test']
    mozharness = test['mozharness']
    worker = taskdesc['worker']

    is_macosx = worker['os'] == 'macosx'
    is_windows = worker['os'] == 'windows'
    is_linux = worker['os'] == 'linux'
    assert is_macosx or is_windows or is_linux

    artifacts = [
        {
            'name': 'public/logs',
            'path': 'logs',
            'type': 'directory'
        },
    ]

    # jittest doesn't have blob_upload_dir
    if test['test-name'] != 'jittest':
        artifacts.append({
            'name': 'public/test_info',
            'path': 'build/blobber_upload_dir',
            'type': 'directory'
        })

    upstream_task = '<build-signing>' if mozharness['requires-signed-builds'] else '<build>'
    installer_url = get_artifact_url(upstream_task, mozharness['build-artifact-name'])

    taskdesc['scopes'].extend(
        ['generic-worker:os-group:{}/{}'.format(
            job['worker-type'],
            group
        ) for group in test['os-groups']])

    worker['os-groups'] = test['os-groups']

    # run-as-administrator is a feature for workers with UAC enabled and as such should not be
    # included in tasks on workers that have UAC disabled. Currently UAC is only enabled on
    # gecko Windows 10 workers, however this may be subject to change. Worker type
    # environment definitions can be found in https://github.com/mozilla-releng/OpenCloudConfig
    # See https://docs.microsoft.com/en-us/windows/desktop/secauthz/user-account-control
    # for more information about UAC.
    if test.get('run-as-administrator', False):
        if job['worker-type'].startswith('aws-provisioner-v1/gecko-t-win10-64'):
            taskdesc['scopes'].extend(
                ['generic-worker:run-as-administrator:{}'.format(job['worker-type'])])
            worker['run-as-administrator'] = True
        else:
            raise Exception('run-as-administrator not supported on {}'.format(job['worker-type']))

    if test['reboot']:
        raise Exception('reboot: {} not supported on generic-worker'.format(test['reboot']))

    worker['max-run-time'] = test['max-run-time']
    worker['artifacts'] = artifacts

    env = worker.setdefault('env', {})
    env['MOZ_AUTOMATION'] = '1'
    env['GECKO_HEAD_REPOSITORY'] = config.params['head_repository']
    env['GECKO_HEAD_REV'] = config.params['head_rev']

    # this list will get cleaned up / reduced / removed in bug 1354088
    if is_macosx:
        env.update({
            'IDLEIZER_DISABLE_SHUTDOWN': 'true',
            'LANG': 'en_US.UTF-8',
            'LC_ALL': 'en_US.UTF-8',
            'MOZ_HIDE_RESULTS_TABLE': '1',
            'MOZ_NODE_PATH': '/usr/local/bin/node',
            'MOZ_NO_REMOTE': '1',
            'NO_FAIL_ON_TEST_ERRORS': '1',
            'PATH': '/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin',
            'SHELL': '/bin/bash',
            'XPCOM_DEBUG_BREAK': 'warn',
            'XPC_FLAGS': '0x0',
            'XPC_SERVICE_NAME': '0',
        })

    if is_windows:
        mh_command = [
            'c:\\mozilla-build\\python\\python.exe',
            '-u',
            'mozharness\\scripts\\' + normpath(mozharness['script'])
        ]
    else:
        # is_linux or is_macosx
        mh_command = [
            'python2.7',
            '-u',
            'mozharness/scripts/' + mozharness['script']
        ]

    for mh_config in mozharness['config']:
        cfg_path = 'mozharness/configs/' + mh_config
        if is_windows:
            cfg_path = normpath(cfg_path)
        mh_command.extend(['--cfg', cfg_path])
    mh_command.extend(mozharness.get('extra-options', []))
    mh_command.extend(['--installer-url', installer_url])
    mh_command.extend(['--test-packages-url', test_packages_url(taskdesc)])
    if mozharness.get('download-symbols'):
        if isinstance(mozharness['download-symbols'], basestring):
            mh_command.extend(['--download-symbols', mozharness['download-symbols']])
        else:
            mh_command.extend(['--download-symbols', 'true'])
    if mozharness.get('include-blob-upload-branch'):
        mh_command.append('--blob-upload-branch=' + config.params['project'])
    mh_command.extend(mozharness.get('extra-options', []))

    # TODO: remove the need for run['chunked']
    if mozharness.get('chunked') or test['chunks'] > 1:
        # Implement mozharness['chunking-args'], modifying command in place
        if mozharness['chunking-args'] == 'this-chunk':
            mh_command.append('--total-chunk={}'.format(test['chunks']))
            mh_command.append('--this-chunk={}'.format(test['this-chunk']))
        elif mozharness['chunking-args'] == 'test-suite-suffix':
            suffix = mozharness['chunk-suffix'].replace('<CHUNK>', str(test['this-chunk']))
            for i, c in enumerate(mh_command):
                if isinstance(c, basestring) and c.startswith('--test-suite'):
                    mh_command[i] += suffix

    if config.params.is_try():
        env['TRY_COMMIT_MSG'] = config.params['message']

    worker['mounts'] = [{
        'directory': '.',
        'content': {
            'artifact': get_artifact_path(taskdesc, 'mozharness.zip'),
            'task-id': {
                'task-reference': '<build>'
            }
        },
        'format': 'zip'
    }]

    if is_windows:
        worker['command'] = [
            {'task-reference': ' '.join(mh_command)}
        ]
    else:  # is_macosx
        mh_command_task_ref = []
        for token in mh_command:
            mh_command_task_ref.append({'task-reference': token})
        worker['command'] = [
            mh_command_task_ref
        ]


@run_job_using('native-engine', 'mozharness-test', schema=mozharness_test_run_schema)
def mozharness_test_on_native_engine(config, job, taskdesc):
    test = taskdesc['run']['test']
    mozharness = test['mozharness']
    worker = taskdesc['worker']
    is_talos = test['suite'] == 'talos' or test['suite'] == 'raptor'
    is_macosx = worker['os'] == 'macosx'

    installer_url = get_artifact_url('<build>', mozharness['build-artifact-name'])
    mozharness_url = get_artifact_url('<build>',
                                      get_artifact_path(taskdesc, 'mozharness.zip'))

    worker['artifacts'] = [{
        'name': prefix.rstrip('/'),
        'path': path.rstrip('/'),
        'type': 'directory',
    } for (prefix, path) in [
        # (artifact name prefix, in-image path relative to homedir)
        ("public/logs/", "workspace/build/logs/"),
        ("public/test", "artifacts/"),
        ("public/test_info/", "workspace/build/blobber_upload_dir/"),
    ]]

    if test['reboot']:
        worker['reboot'] = test['reboot']

    if test['max-run-time']:
        worker['max-run-time'] = test['max-run-time']

    env = worker.setdefault('env', {})
    env.update({
        'GECKO_HEAD_REPOSITORY': config.params['head_repository'],
        'GECKO_HEAD_REV': config.params['head_rev'],
        'MOZHARNESS_CONFIG': ' '.join(mozharness['config']),
        'MOZHARNESS_SCRIPT': mozharness['script'],
        'MOZHARNESS_URL': {'task-reference': mozharness_url},
        'MOZILLA_BUILD_URL': {'task-reference': installer_url},
        "MOZ_NO_REMOTE": '1',
        "XPCOM_DEBUG_BREAK": 'warn',
        "NO_FAIL_ON_TEST_ERRORS": '1',
        "MOZ_HIDE_RESULTS_TABLE": '1',
        "MOZ_NODE_PATH": "/usr/local/bin/node",
        'MOZ_AUTOMATION': '1',
    })
    # talos tests don't need Xvfb
    if is_talos:
        env['NEED_XVFB'] = 'false'

    script = 'test-macosx.sh' if is_macosx else 'test-linux.sh'
    worker['context'] = '{}/raw-file/{}/taskcluster/scripts/tester/{}'.format(
        config.params['head_repository'], config.params['head_rev'], script
    )

    command = worker['command'] = ["./{}".format(script)]
    command.extend([
        {"task-reference": "--installer-url=" + installer_url},
        {"task-reference": "--test-packages-url=" + test_packages_url(taskdesc)},
    ])
    if mozharness.get('include-blob-upload-branch'):
        command.append('--blob-upload-branch=' + config.params['project'])
    command.extend(mozharness.get('extra-options', []))

    # TODO: remove the need for run['chunked']
    if mozharness.get('chunked') or test['chunks'] > 1:
        # Implement mozharness['chunking-args'], modifying command in place
        if mozharness['chunking-args'] == 'this-chunk':
            command.append('--total-chunk={}'.format(test['chunks']))
            command.append('--this-chunk={}'.format(test['this-chunk']))
        elif mozharness['chunking-args'] == 'test-suite-suffix':
            suffix = mozharness['chunk-suffix'].replace('<CHUNK>', str(test['this-chunk']))
            for i, c in enumerate(command):
                if isinstance(c, basestring) and c.startswith('--test-suite'):
                    command[i] += suffix

    if 'download-symbols' in mozharness:
        download_symbols = mozharness['download-symbols']
        download_symbols = {True: 'true', False: 'false'}.get(download_symbols, download_symbols)
        command.append('--download-symbols=' + download_symbols)


@run_job_using('script-engine-autophone', 'mozharness-test', schema=mozharness_test_run_schema)
def mozharness_test_on_script_engine_autophone(config, job, taskdesc):
    test = taskdesc['run']['test']
    mozharness = test['mozharness']
    worker = taskdesc['worker']
    is_talos = test['suite'] == 'talos' or test['suite'] == 'raptor'
    if worker['os'] != 'linux':
        raise Exception('os: {} not supported on script-engine-autophone'.format(worker['os']))

    installer_url = get_artifact_url('<build>', mozharness['build-artifact-name'])
    mozharness_url = get_artifact_url('<build>',
                                      'public/build/mozharness.zip')

    artifacts = [
        # (artifact name prefix, in-image path)
        ("public/test/", "/builds/worker/artifacts"),
        ("public/logs/", "/builds/worker/workspace/build/logs"),
        ("public/test_info/", "/builds/worker/workspace/build/blobber_upload_dir"),
    ]

    worker['artifacts'] = [{
        'name': prefix,
        'path': path,
        'type': 'directory',
    } for (prefix, path) in artifacts]

    if test['reboot']:
        worker['reboot'] = test['reboot']

    worker['env'] = env = {
        'GECKO_HEAD_REPOSITORY': config.params['head_repository'],
        'GECKO_HEAD_REV': config.params['head_rev'],
        'MOZHARNESS_CONFIG': ' '.join(mozharness['config']),
        'MOZHARNESS_SCRIPT': mozharness['script'],
        'MOZHARNESS_URL': {'task-reference': mozharness_url},
        'MOZILLA_BUILD_URL': {'task-reference': installer_url},
        "MOZ_NO_REMOTE": '1',
        "XPCOM_DEBUG_BREAK": 'warn',
        "NO_FAIL_ON_TEST_ERRORS": '1',
        "MOZ_HIDE_RESULTS_TABLE": '1',
        "MOZ_NODE_PATH": "/usr/local/bin/node",
        'MOZ_AUTOMATION': '1',
        'WORKING_DIR': '/builds/worker',
        'WORKSPACE': '/builds/worker/workspace',
        'TASKCLUSTER_WORKER_TYPE': job['worker-type'],
    }

    # for fetch tasks on mobile
    if 'env' in job['worker'] and 'MOZ_FETCHES' in job['worker']['env']:
        env['MOZ_FETCHES'] = job['worker']['env']['MOZ_FETCHES']
        env['MOZ_FETCHES_DIR'] = job['worker']['env']['MOZ_FETCHES_DIR']

    # talos tests don't need Xvfb
    if is_talos:
        env['NEED_XVFB'] = 'false'

    script = 'test-linux.sh'
    worker['context'] = '{}/raw-file/{}/taskcluster/scripts/tester/{}'.format(
        config.params['head_repository'], config.params['head_rev'], script
    )

    command = worker['command'] = ["./{}".format(script)]
    command.extend([
        {"task-reference": "--installer-url=" + installer_url},
        {"task-reference": "--test-packages-url=" + test_packages_url(taskdesc)},
    ])
    if mozharness.get('include-blob-upload-branch'):
        command.append('--blob-upload-branch=' + config.params['project'])
    command.extend(mozharness.get('extra-options', []))

    # TODO: remove the need for run['chunked']
    if mozharness.get('chunked') or test['chunks'] > 1:
        # Implement mozharness['chunking-args'], modifying command in place
        if mozharness['chunking-args'] == 'this-chunk':
            command.append('--total-chunk={}'.format(test['chunks']))
            command.append('--this-chunk={}'.format(test['this-chunk']))
        elif mozharness['chunking-args'] == 'test-suite-suffix':
            suffix = mozharness['chunk-suffix'].replace('<CHUNK>', str(test['this-chunk']))
            for i, c in enumerate(command):
                if isinstance(c, basestring) and c.startswith('--test-suite'):
                    command[i] += suffix

    if 'download-symbols' in mozharness:
        download_symbols = mozharness['download-symbols']
        download_symbols = {True: 'true', False: 'false'}.get(download_symbols, download_symbols)
        command.append('--download-symbols=' + download_symbols)
