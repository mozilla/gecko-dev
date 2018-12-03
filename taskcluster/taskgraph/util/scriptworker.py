# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""Make scriptworker.cot.verify more user friendly by making scopes dynamic.

Scriptworker uses certain scopes to determine which sets of credentials to use.
Certain scopes are restricted by branch in chain of trust verification, and are
checked again at the script level.  This file provides functions to adjust
these scopes automatically by project; this makes pushing to try, forking a
project branch, and merge day uplifts more user friendly.

In the future, we may adjust scopes by other settings as well, e.g. different
scopes for `push-to-candidates` rather than `push-to-releases`, even if both
happen on mozilla-beta and mozilla-release.

Additional configuration is found in the :ref:`graph config <taskgraph-graph-config>`.
"""
from __future__ import absolute_import, print_function, unicode_literals
import functools
import json
import os
import itertools
from copy import deepcopy
from datetime import datetime
import jsone

from .schema import resolve_keyed_by
from .taskcluster import get_artifact_prefix
from .yaml import load_yaml

# constants {{{1
"""Map signing scope aliases to sets of projects.

Currently m-c and DevEdition on m-b use nightly signing; Beta on m-b and m-r
use release signing. These data structures aren't set-up to handle different
scopes on the same repo, so we use a different set of them for DevEdition, and
callers are responsible for using the correct one (by calling the appropriate
helper below). More context on this in https://bugzilla.mozilla.org/show_bug.cgi?id=1358601.

We will need to add esr support at some point. Eventually we want to add
nuance so certain m-b and m-r tasks use dep or nightly signing, and we only
release sign when we have a signed-off set of candidate builds.  This current
approach works for now, though.

This is a list of list-pairs, for ordering.
"""
SIGNING_SCOPE_ALIAS_TO_PROJECT = [[
    'all-nightly-branches', set([
        'mozilla-central',
        'comm-central',
    ])
], [
    'all-release-branches', set([
        'mozilla-beta',
        'mozilla-release',
        'mozilla-esr60',
        'comm-beta',
        'comm-esr60',
    ])
]]

"""Map the signing scope aliases to the actual scopes.
"""
SIGNING_CERT_SCOPES = {
    'all-release-branches': 'signing:cert:release-signing',
    'all-nightly-branches': 'signing:cert:nightly-signing',
    'default': 'signing:cert:dep-signing',
}

DEVEDITION_SIGNING_SCOPE_ALIAS_TO_PROJECT = [[
    'beta', set([
        'mozilla-beta',
    ])
]]

DEVEDITION_SIGNING_CERT_SCOPES = {
    'beta': 'signing:cert:nightly-signing',
    'default': 'signing:cert:dep-signing',
}

"""Map beetmover scope aliases to sets of projects.
"""
BEETMOVER_SCOPE_ALIAS_TO_PROJECT = [[
    'all-nightly-branches', set([
        'mozilla-central',
        'comm-central',
    ])
], [
    'all-release-branches', set([
        'mozilla-beta',
        'mozilla-release',
        'mozilla-esr60',
        'comm-beta',
        'comm-esr60',
    ])
]]

"""Map the beetmover scope aliases to the actual scopes.
"""
BEETMOVER_BUCKET_SCOPES = {
    'all-release-branches': 'beetmover:bucket:release',
    'all-nightly-branches': 'beetmover:bucket:nightly',
    'default': 'beetmover:bucket:dep',
}

"""Map the beetmover tasks aliases to the actual action scopes.
"""
BEETMOVER_ACTION_SCOPES = {
    'nightly': 'beetmover:action:push-to-nightly',
    'default': 'beetmover:action:push-to-candidates',
}


"""Known balrog actions."""
BALROG_ACTIONS = ('submit-locale', 'submit-toplevel', 'schedule')

"""Map balrog scope aliases to sets of projects.

This is a list of list-pairs, for ordering.
"""
BALROG_SCOPE_ALIAS_TO_PROJECT = [[
    'nightly', set([
        'mozilla-central',
        'comm-central'
    ])
], [
    'beta', set([
        'mozilla-beta',
        'comm-beta',
    ])
], [
    'release', set([
        'mozilla-release',
    ])
], [
    'esr60', set([
        'mozilla-esr60',
        'comm-esr60',
    ])
], [
    'esr', set([
        'mozilla-esr52',
    ])
]]

"""Map the balrog scope aliases to the actual scopes.
"""
BALROG_SERVER_SCOPES = {
    'nightly': 'balrog:server:nightly',
    'aurora': 'balrog:server:aurora',
    'beta': 'balrog:server:beta',
    'release': 'balrog:server:release',
    'esr': 'balrog:server:esr',
    'esr60': 'balrog:server:esr',
    'default': 'balrog:server:dep',
}


PUSH_APK_SCOPE_ALIAS_TO_PROJECT = [[
    'central', set([
        'mozilla-central',
    ])
], [
    'beta', set([
        'mozilla-beta',
    ])
], [
    'release', set([
        'mozilla-release',
    ])
]]


PUSH_APK_SCOPES = {
    'central': 'googleplay:aurora',
    'beta': 'googleplay:beta',
    'release': 'googleplay:release',
    'default': 'googleplay:dep',
}


""" The list of the release promotion phases which we send notifications for
"""
RELEASE_NOTIFICATION_PHASES = ('promote', 'push', 'ship')


def add_scope_prefix(config, scope):
    """
    Prepends the scriptworker scope prefix from the :ref:`graph config
    <taskgraph-graph-config>`.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        scope (string): The suffix of the scope

    Returns:
        string: the scope to use.
    """
    return "{prefix}:{scope}".format(
        prefix=config.graph_config['scriptworker']['scope-prefix'],
        scope=scope,
    )


def with_scope_prefix(f):
    """
    Wraps a function, calling :py:func:`add_scope_prefix` on the result of
    calling the wrapped function.

    Args:
        f (callable): A function that takes a ``config`` and some keyword
            arguments, and returns a scope suffix.

    Returns:
        callable: the wrapped function
    """
    @functools.wraps(f)
    def wrapper(config, **kwargs):
        scope_or_scopes = f(config, **kwargs)
        if isinstance(scope_or_scopes, list):
            return map(functools.partial(add_scope_prefix, config), scope_or_scopes)
        else:
            return add_scope_prefix(config, scope_or_scopes)

    return wrapper


# scope functions {{{1
@with_scope_prefix
def get_scope_from_project(config, alias_to_project_map, alias_to_scope_map):
    """Determine the restricted scope from `config.params['project']`.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        alias_to_project_map (list of lists): each list pair contains the
            alias and the set of projects that match.  This is ordered.
        alias_to_scope_map (dict): the alias alias to scope

    Returns:
        string: the scope to use.
    """
    for alias, projects in alias_to_project_map:
        if config.params['project'] in projects and alias in alias_to_scope_map:
            return alias_to_scope_map[alias]
    return alias_to_scope_map['default']


@with_scope_prefix
def get_scope_from_release_type(config, release_type_to_scope_map):
    """Determine the restricted scope from `config.params['target_tasks_method']`.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        release_type_to_scope_map (dict): the maps release types to scopes

    Returns:
        string: the scope to use.
    """
    return release_type_to_scope_map.get(
        config.params['release_type'],
        release_type_to_scope_map['default']
    )


def get_phase_from_target_method(config, alias_to_tasks_map, alias_to_phase_map):
    """Determine the phase from `config.params['target_tasks_method']`.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        alias_to_tasks_map (list of lists): each list pair contains the
            alias and the set of target methods that match. This is ordered.
        alias_to_phase_map (dict): the alias to phase map

    Returns:
        string: the phase to use.
    """
    for alias, tasks in alias_to_tasks_map:
        if config.params['target_tasks_method'] in tasks and alias in alias_to_phase_map:
            return alias_to_phase_map[alias]
    return alias_to_phase_map['default']


@with_scope_prefix
def get_balrog_action_scope(config, action='submit'):
    assert action in BALROG_ACTIONS
    return "balrog:action:{}".format(action)


get_signing_cert_scope = functools.partial(
    get_scope_from_project,
    alias_to_project_map=SIGNING_SCOPE_ALIAS_TO_PROJECT,
    alias_to_scope_map=SIGNING_CERT_SCOPES,
)

get_devedition_signing_cert_scope = functools.partial(
    get_scope_from_project,
    alias_to_project_map=DEVEDITION_SIGNING_SCOPE_ALIAS_TO_PROJECT,
    alias_to_scope_map=DEVEDITION_SIGNING_CERT_SCOPES,
)

get_beetmover_bucket_scope = functools.partial(
    get_scope_from_project,
    alias_to_project_map=BEETMOVER_SCOPE_ALIAS_TO_PROJECT,
    alias_to_scope_map=BEETMOVER_BUCKET_SCOPES,
)

get_beetmover_action_scope = functools.partial(
    get_scope_from_release_type,
    release_type_to_scope_map=BEETMOVER_ACTION_SCOPES,
)

get_balrog_server_scope = functools.partial(
    get_scope_from_project,
    alias_to_project_map=BALROG_SCOPE_ALIAS_TO_PROJECT,
    alias_to_scope_map=BALROG_SERVER_SCOPES,
)

get_push_apk_scope = functools.partial(
    get_scope_from_project,
    alias_to_project_map=PUSH_APK_SCOPE_ALIAS_TO_PROJECT,
    alias_to_scope_map=PUSH_APK_SCOPES,
)


# release_config {{{1
def get_release_config(config):
    """Get the build number and version for a release task.

    Currently only applies to beetmover tasks.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.

    Returns:
        dict: containing both `build_number` and `version`.  This can be used to
            update `task.payload`.
    """
    release_config = {}

    partial_updates = os.environ.get("PARTIAL_UPDATES", "")
    if partial_updates != "" and config.kind in ('release-bouncer-sub',
                                                 'release-bouncer-check',
                                                 'release-update-verify-config',
                                                 'release-secondary-update-verify-config',
                                                 'release-balrog-submit-toplevel',
                                                 'release-secondary-balrog-submit-toplevel',
                                                 'release-mark-as-started'
                                                 ):
        partial_updates = json.loads(partial_updates)
        release_config['partial_versions'] = ', '.join([
            '{}build{}'.format(v, info['buildNumber'])
            for v, info in partial_updates.items()
        ])
        if release_config['partial_versions'] == "{}":
            del release_config['partial_versions']

    release_config['version'] = str(config.params['version'])
    release_config['appVersion'] = str(config.params['app_version'])

    release_config['next_version'] = str(config.params['next_version'])
    release_config['build_number'] = config.params['build_number']
    return release_config


def get_signing_cert_scope_per_platform(build_platform, is_nightly, config):
    if 'devedition' in build_platform:
        return get_devedition_signing_cert_scope(config)
    elif is_nightly or build_platform in ('firefox-source', 'fennec-source', 'thunderbird-source'):
        return get_signing_cert_scope(config)
    else:
        return add_scope_prefix(config, 'signing:cert:dep-signing')


def get_worker_type_for_scope(config, scope):
    """Get the scriptworker type that will accept the given scope.

    Args:
        config (TransformConfig): The configuration for the kind being transformed.
        scope (string): The scope being used.

    Returns:
        string: The worker-type to use.
    """
    for worker_type, scopes in config.graph_config['scriptworker']['worker-types'].items():
        if scope in scopes:
            return worker_type
    raise RuntimeError(
        "Unsupported scriptworker scope {scope}. (supported scopes: {available_scopes})".format(
            scope=scope,
            available_scopes=sorted(
                scope
                for scopes in config.graph_config['scriptworker']['worker-types'].values()
                for scope in scopes
            ),
        )
    )


# generate_beetmover_upstream_artifacts {{{1
def generate_beetmover_upstream_artifacts(job, platform, locale=None, dependencies=None):
    """Generate the upstream artifacts for beetmover, using the artifact map.

    Currently only applies to beetmover tasks.

    Args:
        job (dict): The current job being generated
        dependencies (list): A list of the job's dependency labels.
        platform (str): The current build platform
        locale (str): The current locale being beetmoved.

    Returns:
        list: A list of dictionaries conforming to the upstream_artifacts spec.
    """
    base_artifact_prefix = get_artifact_prefix(job)
    resolve_keyed_by(job, 'attributes.artifact_map', 'artifact map', platform=platform)
    map_config = load_yaml(*os.path.split(job['attributes']['artifact_map']))
    upstream_artifacts = list()

    if not locale:
        locales = map_config['default_locales']
    else:
        locales = [locale]

    if not dependencies:
        dependencies = job['dependencies'].keys()

    for locale, dep in itertools.product(locales, dependencies):
        paths = list()

        for filename in map_config['mapping']:
            if dep not in map_config['mapping'][filename]['from']:
                continue
            if locale != 'en-US' and not map_config['mapping'][filename]['all_locales']:
                continue

            # The next time we look at this file it might be a different locale.
            file_config = deepcopy(map_config['mapping'][filename])
            resolve_keyed_by(file_config, "source_path_modifier",
                             'source path modifier', locale=locale)
            paths.append(os.path.join(
                base_artifact_prefix,
                jsone.render(file_config['source_path_modifier'], {'locale': locale}),
                filename,
            ))

        if not paths:
            continue

        upstream_artifacts.append({
            "taskId": {
                "task-reference": "<{}>".format(dep)
            },
            "taskType": map_config['tasktype_map'].get(dep),
            "paths": sorted(paths),
            "locale": locale,
        })

    return upstream_artifacts


# generate_beetmover_compressed_upstream_artifacts {{{1
def generate_beetmover_compressed_upstream_artifacts(job, dependencies=None):
    """Generate compressed file upstream artifacts for beetmover.

    These artifacts will not be beetmoved directly, but will be
    decompressed from upstream_mapping and the contents beetmoved
    using the `mapping` entry in the artifact map.

    Currently only applies to beetmover tasks.

    Args:
        job (dict): The current job being generated
        dependencies (list): A list of the job's dependency labels.

    Returns:
        list: A list of dictionaries conforming to the upstream_artifacts spec.
    """
    base_artifact_prefix = get_artifact_prefix(job)
    map_config = load_yaml(*os.path.split(job['attributes']['artifact_map']))
    upstream_artifacts = list()

    if not dependencies:
        dependencies = job['dependencies'].keys()

    for dep in dependencies:
        paths = list()

        for filename in map_config['upstream_mapping']:
            if dep not in map_config['upstream_mapping'][filename]['from']:
                continue

            paths.append(os.path.join(
                base_artifact_prefix,
                filename,
            ))

        if not paths:
            continue

        upstream_artifacts.append({
            "taskId": {
                "task-reference": "<{}>".format(dep)
            },
            "taskType": map_config['tasktype_map'].get(dep),
            "paths": sorted(paths),
            "zipExtract": True,
        })

    return upstream_artifacts


# generate_beetmover_artifact_map {{{1
def generate_beetmover_artifact_map(config, job, **kwargs):
    """Generate the beetmover artifact map.

    Currently only applies to beetmover tasks.

    Args:
        config (): Current taskgraph configuration.
        job (dict): The current job being generated
    Common kwargs:
        platform (str): The current build platform
        locale (str): The current locale being beetmoved.

    Returns:
        list: A list of dictionaries containing source->destination
            maps for beetmover.
    """
    platform = kwargs.get('platform', '')
    resolve_keyed_by(job, 'attributes.artifact_map', 'artifact map', platform=platform)
    map_config = load_yaml(*os.path.split(job['attributes']['artifact_map']))
    base_artifact_prefix = map_config.get('base_artifact_prefix', get_artifact_prefix(job))

    artifacts = list()

    dependencies = job['dependencies'].keys()

    if kwargs.get('locale'):
        locales = [kwargs['locale']]
    else:
        locales = map_config['default_locales']

    resolve_keyed_by(map_config, 's3_bucket_paths', 's3_bucket_paths', platform=platform)

    for locale, dep in itertools.product(locales, dependencies):
        paths = dict()
        for filename in map_config['mapping']:
            # Relevancy checks
            if dep not in map_config['mapping'][filename]['from']:
                # We don't get this file from this dependency.
                continue
            if locale != 'en-US' and not map_config['mapping'][filename]['all_locales']:
                # This locale either doesn't produce or shouldn't upload this file.
                continue

            # Filling in destinations

            # deepcopy because the next time we look at this file the locale will differ.
            file_config = deepcopy(map_config['mapping'][filename])

            for field in [
                'destinations',
                'locale_prefix',
                'source_path_modifier',
                'update_balrog_manifest',
                'pretty_name',
                'checksums_path'
            ]:
                resolve_keyed_by(file_config, field, field, locale=locale, platform=platform)

            # This format string should ideally be in the configuration file,
            # but this would mean keeping variable names in sync between code + config.
            destinations = [
                "{s3_bucket_path}/{dest_path}/{locale_prefix}{filename}".format(
                    s3_bucket_path=bucket_path,
                    dest_path=dest_path,
                    locale_prefix=file_config['locale_prefix'],
                    filename=file_config.get('pretty_name', filename),
                )
                for dest_path, bucket_path
                in itertools.product(file_config['destinations'], map_config['s3_bucket_paths'])
            ]
            # Creating map entries

            # Key must be artifact path, to avoid trampling duplicates, such
            # as public/build/target.apk and public/build/en-US/target.apk
            key = os.path.join(
                base_artifact_prefix,
                file_config['source_path_modifier'],
                filename,
            )

            paths[key] = {
                'destinations': destinations,
            }
            if file_config.get('checksums_path'):
                paths[key]['checksums_path'] = file_config['checksums_path']

            # Optional flags.
            if file_config.get('update_balrog_manifest'):
                paths[key]['update_balrog_manifest'] = True
                if file_config.get('balrog_format'):
                    paths[key]['balrog_format'] = file_config['balrog_format']

        if not paths:
            # No files for this dependency/locale combination.
            continue

        # Render all variables for the artifact map

        platforms = deepcopy(map_config['platform_names'])
        if platform:
            for key in platforms.keys():
                resolve_keyed_by(platforms, key, key, platform=platform)

        upload_date = datetime.fromtimestamp(config.params['build_date'])

        kwargs.update({
            'locale': locale,
            'version': config.params['app_version'],
            'branch': config.params['project'],
            'build_number': config.params['build_number'],
            'filename_platform': platforms['filename_platform'],
            'path_platform': platforms['path_platform'],
            'year': upload_date.year,
            'month': upload_date.strftime("%m"),  # zero-pad the month
            'upload_date': upload_date.strftime("%Y-%m-%d-%H-%M-%S")
        })
        paths = jsone.render(paths, kwargs)
        artifacts.append({
            'taskId': {'task-reference': "<{}>".format(dep)},
            'locale': locale,
            'paths': paths,
        })

    return artifacts


# should_use_artifact_map {{{
def should_use_artifact_map(platform, project):
    """Return True if this task uses the beetmover artifact map.

    This function exists solely for the beetmover artifact map
    migration.
    """
    platforms = ['android', 'fennec']
    # FIXME: once we're ready to switch fully to declarative artifacts on other
    # branches, we can expand this
    projects = ['mozilla-central']

    if any([pl in platform for pl in platforms]) and any([pj in project for pj in projects]):
        return True
    return False
