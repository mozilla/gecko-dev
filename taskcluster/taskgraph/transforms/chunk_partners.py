# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Chunk the partner repack tasks by subpartner and locale
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.partners import get_partner_config_by_kind, locales_per_build_platform

transforms = TransformSequence()


repack_ids_by_platform = {}


def _check_repack_ids_by_platform(platform, repack_id):
    """avoid dup chunks, since mac signing and repackages both chunk"""
    if repack_ids_by_platform.get(platform, {}).get(repack_id):
        return True
    repack_ids_by_platform.setdefault(platform, {})['repack_id'] = True


@transforms.add
def chunk_partners(config, jobs):
    partner_configs = get_partner_config_by_kind(config, config.kind)

    for job in jobs:
        dep_job = job['primary-dependency']
        build_platform = dep_job.attributes["build_platform"]
        # already chunked
        if dep_job.task.get('extra', {}).get('repack_id'):
            repack_id = dep_job.task['extra']['repack_id']
            if _check_repack_ids_by_platform(build_platform, repack_id):
                continue
            partner_job = copy.deepcopy(job)
            partner_job.setdefault('extra', {}).setdefault('repack_id', repack_id)
            yield partner_job
            continue
        # not already chunked
        for partner, partner_config in partner_configs.iteritems():
            for sub_partner, cfg in partner_config.iteritems():
                if build_platform not in cfg.get("platforms", []):
                    continue
                locales = locales_per_build_platform(build_platform, cfg.get('locales', []))
                for locale in locales:
                    repack_id = "{}/{}/{}".format(partner, sub_partner, locale)
                    if _check_repack_ids_by_platform(build_platform, repack_id):
                        continue
                    partner_job = copy.deepcopy(job)  # don't overwrite dict values here
                    partner_job.setdefault('extra', {})
                    partner_job['extra']['repack_id'] = repack_id

                    yield partner_job
