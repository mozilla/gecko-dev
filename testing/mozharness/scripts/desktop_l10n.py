#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
"""desktop_l10n.py

This script manages Desktop repacks for nightly builds.
"""

import os
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))  # noqa

from mozharness.base.script import BaseScript
from mozharness.base.vcs.vcsbase import VCSMixin
from mozharness.mozilla.automation import AutomationMixin
from mozharness.mozilla.building.buildbase import (
    get_mozconfig_path,
)
from mozharness.mozilla.l10n.locales import LocalesMixin

try:
    import simplejson as json

    assert json
except ImportError:
    import json


# DesktopSingleLocale {{{1
class DesktopSingleLocale(LocalesMixin, AutomationMixin, VCSMixin, BaseScript):
    """Manages desktop repacks"""

    config_options = [
        [
            [
                "--locale",
            ],
            {
                "action": "extend",
                "dest": "locales",
                "type": "string",
                "help": "Specify the locale(s) to sign and update. Optionally pass"
                " revision separated by colon, en-GB:default.",
            },
        ],
    ]

    def __init__(self, require_config_file=True):
        # fxbuild style:
        buildscript_kwargs = {
            "all_actions": [
                "clone-locales",
                "list-locales",
                "setup",
                "repack",
                "summary",
            ],
            "config": {
                "ignore_locales": ["en-US"],
                "locales_dir": "browser/locales",
                "log_name": "single_locale",
                "git_repository": "https://github.com/mozilla-l10n/firefox-l10n",
            },
        }

        LocalesMixin.__init__(self)
        BaseScript.__init__(
            self,
            config_options=self.config_options,
            require_config_file=require_config_file,
            **buildscript_kwargs,
        )

        self.bootstrap_env = None
        self.upload_env = None

    # Helper methods {{{2
    def query_bootstrap_env(self):
        """returns the env for repacks"""
        if self.bootstrap_env:
            return self.bootstrap_env
        config = self.config
        abs_dirs = self.query_abs_dirs()

        bootstrap_env = self.query_env(
            partial_env=config.get("bootstrap_env"), replace_dict=abs_dirs
        )

        bootstrap_env["L10NBASEDIR"] = abs_dirs["abs_l10n_dir"]
        if self.query_is_nightly():
            # we might set update_channel explicitly
            if config.get("update_channel"):
                update_channel = config["update_channel"]
            else:  # Let's just give the generic channel based on branch.
                update_channel = "nightly-%s" % (config["branch"],)
            if not isinstance(update_channel, bytes):
                update_channel = update_channel.encode("utf-8")
            bootstrap_env["MOZ_UPDATE_CHANNEL"] = update_channel
            self.info(
                "Update channel set to: {}".format(bootstrap_env["MOZ_UPDATE_CHANNEL"])
            )
        self.bootstrap_env = bootstrap_env
        return self.bootstrap_env

    def _query_upload_env(self):
        """returns the environment used for the upload step"""
        if self.upload_env:
            return self.upload_env
        config = self.config

        self.upload_env = self.query_env(partial_env=config.get("upload_env"))
        return self.upload_env

    def query_l10n_env(self):
        l10n_env = self._query_upload_env().copy()
        l10n_env.update(self.query_bootstrap_env())
        return l10n_env

    # Actions {{{2
    def clone_locales(self):
        self.pull_locale_source()

    def setup(self):
        """setup step"""
        self._copy_mozconfig()
        self._mach_configure()

    def _copy_mozconfig(self):
        """copies the mozconfig file into abs_src_dir/.mozconfig
        and logs the content
        """
        config = self.config
        dirs = self.query_abs_dirs()
        src = get_mozconfig_path(self, config, dirs)
        dst = os.path.join(dirs["abs_src_dir"], ".mozconfig")
        self.copyfile(src, dst)
        self.read_from_file(dst, verbose=True)

    def _mach(self, target, env, halt_on_failure=True, output_parser=None):
        dirs = self.query_abs_dirs()
        mach = self._get_mach_executable()
        return self.run_command(
            mach + target,
            halt_on_failure=True,
            env=env,
            cwd=dirs["abs_src_dir"],
            output_parser=None,
        )

    def _mach_configure(self):
        """calls mach configure"""
        env = self.query_bootstrap_env()
        target = ["configure"]
        return self._mach(target=target, env=env)

    def _get_mach_executable(self):
        return [sys.executable, "mach"]

    def repack(self):
        env = self.query_bootstrap_env()
        return self._mach(
            target=[
                "repackage-single-locales",
                "--verbose",
                "--dest",
                self.config["upload_env"]["UPLOAD_PATH"],
                "--locales",
            ]
            + list(sorted(self.query_locales())),
            env=env,
        )


# main {{{
if __name__ == "__main__":
    single_locale = DesktopSingleLocale()
    single_locale.run_and_exit()
