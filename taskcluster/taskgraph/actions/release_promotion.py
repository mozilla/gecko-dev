# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import os

from .registry import register_callback_action

from .util import find_decision_task, find_existing_tasks_from_previous_kinds
from taskgraph.util.hg import find_hg_revision_push_info
from taskgraph.util.taskcluster import get_artifact
from taskgraph.util.partials import populate_release_history
from taskgraph.util.partners import (
    EMEFREE_BRANCHES,
    PARTNER_BRANCHES,
    fix_partner_config,
    get_partner_config_by_url,
    get_partner_url_config,
    get_token
)
from taskgraph.taskgraph import TaskGraph
from taskgraph.decision import taskgraph_decision
from taskgraph.parameters import Parameters
from taskgraph.util.attributes import RELEASE_PROMOTION_PROJECTS


RELEASE_PROMOTION_SIGNOFFS = ('mar-signing', )


def is_release_promotion_available(parameters):
    return parameters['project'] in RELEASE_PROMOTION_PROJECTS


def get_partner_config(partner_url_config, github_token):
    partner_config = {}
    for kind, url in partner_url_config.items():
        partner_config[kind] = get_partner_config_by_url(url, kind, github_token)
    return partner_config


def get_signoff_properties():
    props = {}
    for signoff in RELEASE_PROMOTION_SIGNOFFS:
        props[signoff] = {
            'type': 'string',
        }
    return props


def get_required_signoffs(input, parameters):
    input_signoffs = set(input.get('required_signoffs', []))
    params_signoffs = set(parameters['required_signoffs'] or [])
    return sorted(list(input_signoffs | params_signoffs))


def get_signoff_urls(input, parameters):
    signoff_urls = parameters['signoff_urls']
    signoff_urls.update(input.get('signoff_urls', {}))
    return signoff_urls


def get_flavors(graph_config, param):
    """
    Get all flavors with the given parameter enabled.
    """
    promotion_flavors = graph_config['release-promotion']['flavors']
    return sorted([
        flavor for (flavor, config) in promotion_flavors.items()
        if config.get(param, False)
    ])


@register_callback_action(
    name='release-promotion',
    title='Release Promotion',
    symbol='${input.release_promotion_flavor}',
    description="Promote a release.",
    order=500,
    context=[],
    available=is_release_promotion_available,
    schema=lambda graph_config: {
        'type': 'object',
        'properties': {
            'build_number': {
                'type': 'integer',
                'default': 1,
                'minimum': 1,
                'title': 'The release build number',
                'description': ('The release build number. Starts at 1 per '
                                'release version, and increments on rebuild.'),
            },
            'do_not_optimize': {
                'type': 'array',
                'description': ('Optional: a list of labels to avoid optimizing out '
                                'of the graph (to force a rerun of, say, '
                                'funsize docker-image tasks).'),
                'items': {
                    'type': 'string',
                },
            },
            'revision': {
                'type': 'string',
                'title': 'Optional: revision to promote',
                'description': ('Optional: the revision to promote. If specified, '
                                'and if neither `pushlog_id` nor `previous_graph_kinds` '
                                'is specified, find the `pushlog_id using the '
                                'revision.'),
            },
            'release_promotion_flavor': {
                'type': 'string',
                'description': 'The flavor of release promotion to perform.',
                'enum': sorted(graph_config['release-promotion']['flavors'].keys()),
            },
            'rebuild_kinds': {
                'type': 'array',
                'description': ('Optional: an array of kinds to ignore from the previous '
                                'graph(s).'),
                'items': {
                    'type': 'string',
                },
            },
            'previous_graph_ids': {
                'type': 'array',
                'description': ('Optional: an array of taskIds of decision or action '
                                'tasks from the previous graph(s) to use to populate '
                                'our `previous_graph_kinds`.'),
                'items': {
                    'type': 'string',
                },
            },
            'version': {
                'type': 'string',
                'description': ('Optional: override the version for release promotion. '
                                "Occasionally we'll land a taskgraph fix in a later "
                                'commit, but want to act on a build from a previous '
                                'commit. If a version bump has landed in the meantime, '
                                'relying on the in-tree version will break things.'),
                'default': '',
            },
            'next_version': {
                'type': 'string',
                'description': ('Next version. Required in the following flavors: '
                                '{}'.format(get_flavors(graph_config, 'version-bump'))),
                'default': '',
            },

            # Example:
            #   'partial_updates': {
            #       '38.0': {
            #           'buildNumber': 1,
            #           'locales': ['de', 'en-GB', 'ru', 'uk', 'zh-TW']
            #       },
            #       '37.0': {
            #           'buildNumber': 2,
            #           'locales': ['de', 'en-GB', 'ru', 'uk']
            #       }
            #   }
            'partial_updates': {
                'type': 'object',
                'description': ('Partial updates. Required in the following flavors: '
                                '{}'.format(get_flavors(graph_config, 'partial-updates'))),
                'default': {},
                'additionalProperties': {
                    'type': 'object',
                    'properties': {
                        'buildNumber': {
                            'type': 'number',
                        },
                        'locales': {
                            'type': 'array',
                            'items':  {
                                'type': 'string',
                            },
                        },
                    },
                    'required': [
                        'buildNumber',
                        'locales',
                    ],
                    'additionalProperties': False,
                }
            },
            'release_eta': {
                'type': 'string',
                'default': '',
            },
            'release_enable_partners': {
                'type': 'boolean',
                'default': False,
                'description': ('Toggle for creating partner repacks'),
            },
            'release_partner_build_number': {
                'type': 'integer',
                'default': 1,
                'minimum': 1,
                'description': ('The partner build number. This translates to, e.g. '
                                '`v1` in the path. We generally only have to '
                                'bump this on off-cycle partner rebuilds.'),
            },
            'release_partners': {
                'type': 'array',
                'description': ('A list of partners to repack, or if null or empty then use '
                                'the current full set'),
                'items': {
                    'type': 'string',
                }
            },
            'release_partner_config': {
                'type': 'object',
                'description': ('Partner configuration to use for partner repacks.'),
                'properties': {},
                'additionalProperties': True,
            },
            'release_enable_emefree': {
                'type': 'boolean',
                'default': False,
                'description': ('Toggle for creating EME-free repacks'),
            },
            'required_signoffs': {
                'type': 'array',
                'description': ('The flavor of release promotion to perform.'),
                'items': {
                    'enum': RELEASE_PROMOTION_SIGNOFFS,
                }
            },
            'signoff_urls': {
                'type': 'object',
                'default': {},
                'additionalProperties': False,
                'properties': get_signoff_properties(),
            },
        },
        "required": ['release_promotion_flavor', 'build_number'],
    }
)
def release_promotion_action(parameters, graph_config, input, task_group_id, task_id, task):
    release_promotion_flavor = input['release_promotion_flavor']
    promotion_config = graph_config['release-promotion']['flavors'][release_promotion_flavor]
    release_history = {}
    product = promotion_config['product']

    next_version = str(input.get('next_version') or '')
    if promotion_config.get('version-bump', False):
        # We force str() the input, hence the 'None'
        if next_version in ['', 'None']:
            raise Exception(
                "`next_version` property needs to be provided for `{}` "
                "target.".format(release_promotion_flavor)
            )

    if promotion_config.get('partial-updates', False):
        partial_updates = json.dumps(input.get('partial_updates', {}))
        if partial_updates == "{}":
            raise Exception(
                "`partial_updates` property needs to be provided for `{}`"
                "target.".format(release_promotion_flavor)
            )
        balrog_prefix = product.title()
        os.environ['PARTIAL_UPDATES'] = partial_updates
        release_history = populate_release_history(
            balrog_prefix, parameters['project'],
            partial_updates=input['partial_updates']
        )

    target_tasks_method = promotion_config['target-tasks-method'].format(
        project=parameters['project']
    )
    rebuild_kinds = input.get(
        'rebuild_kinds', promotion_config.get('rebuild-kinds', [])
    )
    do_not_optimize = input.get(
        'do_not_optimize', promotion_config.get('do-not-optimize', [])
    )
    release_enable_partners = input.get(
        'release_enable_partners',
        parameters['project'] in PARTNER_BRANCHES and product in ('firefox',)
    )
    release_enable_emefree = input.get(
        'release_enable_emefree',
        parameters['project'] in EMEFREE_BRANCHES and product in ('firefox',)
    )

    # make parameters read-write
    parameters = dict(parameters)
    # Build previous_graph_ids from ``previous_graph_ids``, ``pushlog_id``,
    # or ``revision``.
    previous_graph_ids = input.get('previous_graph_ids')
    if not previous_graph_ids:
        revision = input.get('revision')
        if not parameters['pushlog_id']:
            repo_param = '{}head_repository'.format(graph_config['project-repo-param-prefix'])
            push_info = find_hg_revision_push_info(
                repository=parameters[repo_param], revision=revision)
            parameters['pushlog_id'] = push_info['pushid']
        previous_graph_ids = [find_decision_task(parameters, graph_config)]

    # Download parameters from the first decision task
    parameters = get_artifact(previous_graph_ids[0], "public/parameters.yml")
    # Download and combine full task graphs from each of the previous_graph_ids.
    # Sometimes previous relpro action tasks will add tasks, like partials,
    # that didn't exist in the first full_task_graph, so combining them is
    # important. The rightmost graph should take precedence in the case of
    # conflicts.
    combined_full_task_graph = {}
    for graph_id in previous_graph_ids:
        full_task_graph = get_artifact(graph_id, "public/full-task-graph.json")
        combined_full_task_graph.update(full_task_graph)
    _, combined_full_task_graph = TaskGraph.from_json(combined_full_task_graph)
    parameters['existing_tasks'] = find_existing_tasks_from_previous_kinds(
        combined_full_task_graph, previous_graph_ids, rebuild_kinds
    )
    parameters['do_not_optimize'] = do_not_optimize
    parameters['target_tasks_method'] = target_tasks_method
    parameters['build_number'] = int(input['build_number'])
    parameters['next_version'] = next_version
    parameters['release_history'] = release_history
    if promotion_config.get('is-rc'):
        parameters['release_type'] += '-rc'
    parameters['release_eta'] = input.get('release_eta', '')
    parameters['release_enable_partners'] = release_enable_partners
    parameters['release_partners'] = input.get('release_partners')
    parameters['release_enable_emefree'] = release_enable_emefree
    parameters['release_product'] = product
    # When doing staging releases on try, we still want to re-use tasks from
    # previous graphs.
    parameters['optimize_target_tasks'] = True

    partner_config = input.get('release_partner_config')
    if not partner_config and (release_enable_emefree or release_enable_partners):
        partner_url_config = get_partner_url_config(
            parameters, graph_config, enable_emefree=release_enable_emefree,
            enable_partners=release_enable_partners
        )
        github_token = get_token(parameters)
        partner_config = get_partner_config(partner_url_config, github_token)

    if input.get('release_partner_build_number'):
        parameters['release_partner_build_number'] = input['release_partner_build_number']

    if partner_config:
        parameters['release_partner_config'] = fix_partner_config(partner_config)

    if input['version']:
        parameters['version'] = input['version']

    parameters['required_signoffs'] = get_required_signoffs(input, parameters)
    parameters['signoff_urls'] = get_signoff_urls(input, parameters)

    # make parameters read-only
    parameters = Parameters(**parameters)

    taskgraph_decision({'root': graph_config.root_dir}, parameters=parameters)
