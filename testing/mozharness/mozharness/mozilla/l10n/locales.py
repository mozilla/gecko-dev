#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Localization.
"""

import os
from urlparse import urljoin
import sys
from copy import deepcopy

sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.config import parse_config_file
from mozharness.base.errors import PythonErrorList
from mozharness.base.parallel import ChunkingMixin


# LocalesMixin {{{1
class LocalesMixin(ChunkingMixin):
    def __init__(self, **kwargs):
        """ Mixins generally don't have an __init__.
        This breaks super().__init__() for children.
        However, this is needed to override the query_abs_dirs()
        """
        self.abs_dirs = None
        self.locales = None
        self.gecko_locale_revisions = None

    def query_locales(self):
        if self.locales is not None:
            return self.locales
        c = self.config
        locales = c.get("locales", None)
        ignore_locales = c.get("ignore_locales", [])
        additional_locales = c.get("additional_locales", [])

        if locales is None:
            locales = []
            if 'locales_file' in c:
                # Best way to get abs/relative path to this?
                locales_file = os.path.join(c['base_work_dir'], c['work_dir'],
                                            c['locales_file'])
                locales = self.parse_locales_file(locales_file)
            else:
                self.fatal("No way to determine locales!")
        for locale in ignore_locales:
            if locale in locales:
                self.debug("Ignoring locale %s." % locale)
                locales.remove(locale)
        for locale in additional_locales:
            if locale not in locales:
                self.debug("Adding locale %s." % locale)
                locales.append(locale)
        if locales is None:
            return
        if 'total_locale_chunks' and 'this_locale_chunk' in c:
            self.debug("Pre-chunking locale list: %s" % str(locales))
            locales = self.query_chunked_list(locales,
                                              c['this_locale_chunk'],
                                              c['total_locale_chunks'],
                                              sort=True)
            self.debug("Post-chunking locale list: %s" % locales)
        self.locales = locales
        return self.locales

    def list_locales(self):
        """ Stub action method.
        """
        self.info("Locale list: %s" % str(self.query_locales()))

    def parse_locales_file(self, locales_file):
        locales = []
        c = self.config
        platform = c.get("locales_platform", None)

        if locales_file.endswith('json'):
            locales_json = parse_config_file(locales_file)
            self.locale_dict = {}
            for locale in locales_json.keys():
                if isinstance(locales_json[locale], dict):
                    if platform and platform not in locales_json[locale]['platforms']:
                        continue
                    self.locale_dict[locale] = locales_json[locale]['revision']
                else:
                    # some other way of getting this?
                    self.locale_dict[locale] = 'default'
                locales.append(locale)
        else:
            locales = self.read_from_file(locales_file).split()
        return locales

    def run_compare_locales(self, locale, halt_on_failure=False):
        dirs = self.query_abs_dirs()
        compare_locales_script = os.path.join(dirs['abs_compare_locales_dir'],
                                              'scripts', 'compare-locales')
        env = self.query_env(partial_env={'PYTHONPATH':
                             os.path.join(dirs['abs_compare_locales_dir'],
                                          'lib')})
        compare_locales_error_list = list(PythonErrorList)
        self.rmtree(dirs['abs_merge_dir'])
        self.mkdir_p(dirs['abs_merge_dir'])
        command = "python %s -m %s l10n.ini %s %s" % (compare_locales_script,
                  dirs['abs_merge_dir'], dirs['abs_l10n_dir'], locale)
        self.info("*** BEGIN compare-locales %s" % locale)
        status = self.run_command(command, error_list=compare_locales_error_list,
                                  cwd=dirs['abs_locales_src_dir'], env=env,
                                  halt_on_failure=halt_on_failure)
        self.info("*** END compare-locales %s" % locale)
        return status

    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(LocalesMixin, self).query_abs_dirs()
        c = self.config
        dirs = {}
        dirs['abs_work_dir'] = os.path.join(c['base_work_dir'],
                                            c['work_dir'])
        # TODO prettify this up later
        if 'l10n_dir' in c:
            dirs['abs_l10n_dir'] = os.path.join(dirs['abs_work_dir'],
                                                c['l10n_dir'])
        if 'mozilla_dir' in c:
            dirs['abs_mozilla_dir'] = os.path.join(dirs['abs_work_dir'],
                                                   c['mozilla_dir'])
            dirs['abs_locales_src_dir'] = os.path.join(dirs['abs_mozilla_dir'],
                                                       c['locales_dir'])
            dirs['abs_l10n_dir'] = os.path.join(dirs['abs_work_dir'],
                                                c['l10n_dir'])
        if 'objdir' in c:
            if os.path.isabs(c['objdir']):
                dirs['abs_objdir'] = c['objdir']
            else:
                dirs['abs_objdir'] = os.path.join(dirs['abs_mozilla_dir'],
                                                  c['objdir'])
            dirs['abs_merge_dir'] = os.path.join(dirs['abs_objdir'],
                                                 'merged')
            dirs['abs_locales_dir'] = os.path.join(dirs['abs_objdir'],
                                                   c['locales_dir'])
        dirs['abs_compare_locales_dir'] = os.path.join(dirs['abs_work_dir'],
                                                       'compare-locales')
        for key in dirs.keys():
            if key not in abs_dirs:
                abs_dirs[key] = dirs[key]
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    # This requires self to inherit a VCSMixin.
    def pull_locale_source(self, hg_l10n_base=None, parent_dir=None, vcs='hg'):
        c = self.config
        if not hg_l10n_base:
            hg_l10n_base = c['hg_l10n_base']
        if parent_dir is None:
            parent_dir = self.query_abs_dirs()['abs_l10n_dir']
        self.mkdir_p(parent_dir)
        repos = []
        replace_dict = {}
        # This block is to allow for pulling buildbot-configs in Fennec
        # release builds, since we don't pull it in MBF anymore.
        if c.get("l10n_repos"):
            if c.get("user_repo_override"):
                replace_dict['user_repo_override'] = c['user_repo_override']
                for repo_dict in deepcopy(c['l10n_repos']):
                    repo_dict['repo'] = repo_dict['repo'] % replace_dict
                    repos.append(repo_dict)
            else:
                repos = c.get("l10n_repos")
            self.vcs_checkout_repos(repos, tag_override=c.get('tag_override'))
        # Pull locales
        locales = self.query_locales()
        locale_repos = []
        if c.get("user_repo_override"):
            hg_l10n_base = hg_l10n_base % {"user_repo_override": c["user_repo_override"]}
        for locale in locales:
            tag = c.get('hg_l10n_tag', 'default')
            if hasattr(self, 'locale_dict'):
                tag = self.locale_dict[locale]
            locale_repos.append({
                'repo': "%s/%s" % (hg_l10n_base, locale),
                'tag': tag,
                'vcs': vcs
            })
        revs = self.vcs_checkout_repos(repo_list=locale_repos,
                                       parent_dir=parent_dir,
                                       tag_override=c.get('tag_override'))
        self.gecko_locale_revisions = revs


# GaiaLocalesMixin {{{1
class GaiaLocalesMixin(object):
    gaia_locale_revisions = None

    def pull_gaia_locale_source(self, l10n_config, locales, base_dir):
        root = l10n_config['root']
        # urljoin will strip the last part of root if it doesn't end with "/"
        if not root.endswith('/'):
            root = root + '/'
        vcs = l10n_config['vcs']
        env = l10n_config.get('env', {})
        repos = []
        for locale in locales:
            repos.append({
                'repo': urljoin(root, locale),
                'dest': locale,
                'vcs': vcs,
                'env': env,
            })
        self.gaia_locale_revisions = self.vcs_checkout_repos(repo_list=repos, parent_dir=base_dir)


# __main__ {{{1

if __name__ == '__main__':
    pass
