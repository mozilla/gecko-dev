# this is a dict of branch specific keys/values. As this fills up and more
# fx build factories are ported, we might deal with this differently

# we should be able to port this in-tree and have the respective repos and
# revisions handle what goes on in here. Tracking: bug 978510

# example config and explanation of how it works:
# config = {
#     # if a branch matches a key below, override items in self.config with
#     # items in the key's value.
#     # this override can be done for every platform or at a platform level
#     '<branch-name>': {
#         # global config items (applies to all platforms and build types)
#         'repo_path': "projects/<branch-name>",
#         'graph_server_branch_name': "Firefox",
#
#         # platform config items (applies to specific platforms)
#         'platform_overrides': {
#             # if a platform matches a key below, override items in
#             # self.config with items in the key's value
#             'linux64-debug': {
#                 'upload_symbols': False,
#             },
#             'win64': {
#                 'enable_checktests': False,
#             },
#         }
#     },
# }

config = {
    ### release branches
    "mozilla-central": {
        "repo_path": 'mozilla-central',
        "update_channel": "nightly",
        "graph_server_branch_name": "Firefox",
        'use_branch_in_symbols_extra_buildid': False,
    },
    'mozilla-release': {
        'repo_path': 'releases/mozilla-release',
        # TODO I think we can remove update_channel since we don't run
        # nightlies for mozilla-release
        'update_channel': 'release',
        'branch_uses_per_checkin_strategy': True,
        'use_branch_in_symbols_extra_buildid': False,
    },
    'mozilla-beta': {
        'repo_path': 'releases/mozilla-beta',
        # TODO I think we can remove update_channel since we don't run
        # nightlies for mozilla-beta
        'update_channel': 'beta',
        'branch_uses_per_checkin_strategy': True,
        'use_branch_in_symbols_extra_buildid': False,
    },
    'mozilla-aurora': {
        'repo_path': 'releases/mozilla-aurora',
        'update_channel': 'aurora',
        'branch_uses_per_checkin_strategy': True,
        'use_branch_in_symbols_extra_buildid': False,
    },
    'mozilla-esr31': {
        'repo_path': 'releases/mozilla-esr31',
        'update_channel': 'nightly-esr31',
        'branch_uses_per_checkin_strategy': True,
        'use_branch_in_symbols_extra_buildid': False,
    },
    'mozilla-b2g30_v1_4': {
        'repo_path': 'releases/mozilla-b2g30_v1_4',
        'use_branch_in_symbols_extra_buildid': False,
        'update_channel': 'nightly-b2g30',
        'graph_server_branch_name': 'Mozilla-B2g30-v1.4',
    },
    'mozilla-b2g32_v2_0': {
        'repo_path': 'releases/mozilla-b2g32_v2_0',
        'use_branch_in_symbols_extra_buildid': False,
        'update_channel': 'nightly-b2g32',
        'graph_server_branch_name': 'Mozilla-B2g32-v2.0',
    },
    'try': {
        'repo_path': 'try',
        'clone_by_revision': True,
        'clone_with_purge': True,
        'tinderbox_build_dir': '%(who)s-%(got_revision)s',
        'to_tinderbox_dated': False,
        'include_post_upload_builddir': True,
        'release_to_try_builds': True,
        'use_branch_in_symbols_extra_buildid': False,
        'stage_username': 'trybld',
        'stage_ssh_key': 'trybld_dsa',
        'branch_supports_uploadsymbols': False,
    },

    ### project branches
    'b2g-inbound': {
        'repo_path': 'integration/b2g-inbound',
    },
    'fx-team': {
        'repo_path': 'integration/fx-team',
    },
    'gum': {
        'branch_uses_per_checkin_strategy': True,
    },
    'mozilla-inbound': {
        'repo_path': 'integration/mozilla-inbound',
    },
    'services-central': {
        'repo_path': 'services/services-central',
    },
    'ux': {
        "graph_server_branch_name": "UX",
    },

    ### other branches that do not require anything special:
    # 'alder': {},
    # 'ash': {},
    # 'birch': {},
    # 'build-system': {}
    # 'cedar': {},
    # "cypress": {},
    # 'date': {},
    # 'elm': {},
    # 'fig': {},
    # 'graphics': {}
    # 'holly': {},
    # 'jamun': {},
    # 'larch': {},
    # 'maple': {},
    # 'oak': {},
    # 'pine': {}
}
