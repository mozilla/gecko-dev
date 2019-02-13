#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""firefox_ui_updates.py

Author: Armen Zambrano G.
"""
import copy
import os
import re
import urllib
import urllib2
import sys

# load modules from parent dir
sys.path.insert(1, os.path.dirname(sys.path[0]))

from mozharness.base.script import PreScriptAction
from mozharness.mozilla.testing.firefox_ui_tests import FirefoxUITests
from mozharness.mozilla.buildbot import TBPL_SUCCESS, TBPL_WARNING, EXIT_STATUS_DICT

INSTALLER_SUFFIXES = ('.tar.bz2', '.zip', '.dmg', '.exe', '.apk', '.tar.gz')

class FirefoxUIUpdates(FirefoxUITests):
    # This will be a list containing one item per release based on configs
    # from tools/release/updates/*cfg
    releases = None
    channel = None
    harness_extra_args = [
        [['--update-allow-mar-channel'], {
            'dest': 'update_allow_mar_channel',
            'help': 'Additional MAR channel to be allowed for updates, e.g. '
                    '"firefox-mozilla-beta" for updating a release build to '
                    'the latest beta build.',
        }],
        [['--update-channel'], {
            'dest': 'update_channel',
            'help': 'Update channel to use.',
        }],
        [['--update-target-version'], {
            'dest': 'update_target_version',
            'help': 'Version of the updated build.',
        }],
        [['--update-target-buildid'], {
            'dest': 'update_target_buildid',
            'help': 'Build ID of the updated build',
        }],
        [['--symbols-path=SYMBOLS_PATH'], {
            'dest': 'symbols_path',
            'help': 'absolute path to directory containing breakpad '
                    'symbols, or the url of a zip file containing symbols.',
        }],
    ]


    def __init__(self):
        config_options = [
            [['--tools-repo'], {
                'dest': 'tools_repo',
                'default': 'http://hg.mozilla.org/build/tools',
                'help': 'Which tools repo to check out',
            }],
            [['--tools-tag'], {
                'dest': 'tools_tag',
                'help': 'Which revision/tag to use for the tools repository.',
            }],
            [['--update-verify-config'], {
                'dest': 'update_verify_config',
                'help': 'Which update verify config file to use.',
            }],
            [['--this-chunk'], {
                'dest': 'this_chunk',
                'default': 1,
                'help': 'What chunk of locales to process.',
            }],
            [['--total-chunks'], {
                'dest': 'total_chunks',
                'default': 1,
                'help': 'Total chunks to dive the locales into.',
            }],
            [['--dry-run'], {
                'dest': 'dry_run',
                'default': False,
                'help': 'Only show what was going to be tested.',
            }],
            [["--build-number"], {
                "dest": "build_number",
                "help": "Build number of release, eg: 2",
            }],
            # These are options when we don't use the releng update config file
            [['--installer-url'], {
                'dest': 'installer_url',
                'help': 'Point to an installer to download and test against.',
            }],
            [['--installer-path'], {
                'dest': 'installer_path',
                'help': 'Point to an installer to test against.',
            }],
        ] + copy.deepcopy(self.harness_extra_args)

        super(FirefoxUIUpdates, self).__init__(
            config_options=config_options,
            all_actions=[
                'clobber',
                'checkout',
                'create-virtualenv',
                'determine-testing-configuration',
                'run-tests',
            ],
            append_env_variables_from_configs=True,
        )

        dirs = self.query_abs_dirs()

        if self.config.get('tools_tag') is None:
            # We want to make sure that anyone trying to reproduce a job will
            # is using the exact tools tag for reproducibility's sake
            self.fatal('Make sure to specify the --tools-tag')

        self.tools_repo = self.config['tools_repo']
        self.tools_tag = self.config['tools_tag']

        if self.config.get('update_verify_config'):
            self.update_verify_config = self.config['update_verify_config']
            self.updates_config_file = os.path.join(
                dirs['abs_tools_dir'], 'release', 'updates',
                self.config['update_verify_config']
            )
        else:
            self.fatal('Make sure to specify --update-verify-config. '
                       'See under the directory release/updates in %s.' % self.tools_repo)

        self.installer_url = self.config.get('installer_url')
        self.installer_path = self.config.get('installer_path')

        if self.installer_path:
            if not os.path.exists(self.installer_path):
                self.critical('Please make sure that the path to the installer exists.')
                exit(1)

        assert 'update_verify_config' in self.config or self.installer_url or self.installer_path, \
            'Either specify --update-verify-config, --installer-url or --installer-path.'


    def query_abs_dirs(self):
        if self.abs_dirs:
            return self.abs_dirs
        abs_dirs = super(FirefoxUIUpdates, self).query_abs_dirs()

        dirs = {
            'abs_tools_dir': os.path.join(abs_dirs['abs_work_dir'], 'tools'),
        }

        abs_dirs.update(dirs)
        self.abs_dirs = abs_dirs
        return self.abs_dirs

    def checkout(self):
        '''
        This checkouts the tools repo because it contains the configuration
        files about which locales to test.

        We also checkout firefox_ui_tests and update to the right branch
        for it.
        '''
        super(FirefoxUIUpdates, self).checkout()
        dirs = self.query_abs_dirs()

        self.vcs_checkout(
            repo=self.tools_repo,
            dest=dirs['abs_tools_dir'],
            revision=self.tools_tag,
            vcs='hgtool'
        )


    def determine_testing_configuration(self):
        '''
        This method builds a testing matrix either based on an update verification
        configuration file under the tools repo (release/updates/*.cfg)
        OR it skips it when we use --installer-url --installer-path

        Each release info line of the update verification files look similar to the following.

        NOTE: This shows each pair of information as a new line but in reality
        there is one white space separting them. We only show the values we care for.

            release="38.0"
            platform="Linux_x86_64-gcc3"
            build_id="20150429135941"
            locales="ach af ... zh-TW"
            channel="beta-localtest"
            from="/firefox/releases/38.0b9/linux-x86_64/%locale%/firefox-38.0b9.tar.bz2"
            ftp_server_from="http://stage.mozilla.org/pub/mozilla.org"

        We will store this information in self.releases as a list of releases.

        NOTE: We will talk of full and quick releases. Full release info normally contains a subset
        of all locales (except for the most recent releases). A quick release has all locales,
        however, it misses the fields 'from' and 'ftp_server_from'.
        Both pairs of information complement each other but differ in such manner.
        '''
        if self.installer_url or self.installer_path:
            return

        dirs = self.query_abs_dirs()
        assert os.path.exists(dirs['abs_tools_dir']), \
            "Without the tools/ checkout we can't use releng's config parser."

        # Import the config parser
        sys.path.insert(1, os.path.join(dirs['abs_tools_dir'], 'lib', 'python'))
        from release.updates.verify import UpdateVerifyConfig

        uvc = UpdateVerifyConfig()
        uvc.read(self.updates_config_file)
        self.channel = uvc.channel

        # Filter out any releases that are less than Gecko 38
        uvc.releases = [r for r in uvc.releases \
                if int(r["release"].split('.')[0]) >= 38]

        temp_releases = []
        for rel_info in uvc.releases:
            # This is the full release info
            if 'from' in rel_info and rel_info['from'] is not None:
                # Let's find the associated quick release which contains the remaining locales
                # for all releases except for the most recent release which contain all locales
                quick_release = uvc.getRelease(build_id=rel_info['build_id'], from_path=None)
                if quick_release != {}:
                    rel_info['locales'] = sorted(rel_info['locales'] + quick_release['locales'])
                temp_releases.append(rel_info)

        uvc.releases = temp_releases
        chunked_config = uvc.getChunk(
            chunks=int(self.config['total_chunks']),
            thisChunk=int(self.config['this_chunk'])
        )

        self.releases = chunked_config.releases


    def _modify_url(self, rel_info):
        # This is a temporary hack to find crash symbols. It should be replaced
        # with something that doesn't make wild guesses about where symbol
        # packages are.
        # We want this:
        # https://ftp.mozilla.org/pub/mozilla.org/firefox/candidates/40.0b1-candidates/build1/mac/en-US/Firefox%2040.0b1.crashreporter-symbols.zip
        # https://ftp.mozilla.org/pub/mozilla.org//firefox/releases/40.0b1/mac/en-US/Firefox%2040.0b1.crashreporter-symbols.zip
        installer_from = rel_info['from']
        version = (re.search('/firefox/releases/(%s.*)\/.*\/.*\/.*' % rel_info['release'], installer_from)).group(1)

        temp_from = installer_from.replace(version, '%s-candidates/build%s' % (version, self.config["build_number"]), 1).replace('releases', 'candidates')
        temp_url = rel_info["ftp_server_from"] + urllib.quote(temp_from.replace('%locale%', 'en-US'))
        self.info('Installer url under stage/candidates dir %s' % temp_url)

        return temp_url


    def _query_symbols_url(self, installer_url):
        for suffix in INSTALLER_SUFFIXES:
            if installer_url.endswith(suffix):
                symbols_url = installer_url[:-len(suffix)] + '.crashreporter-symbols.zip'
                continue

        if symbols_url:
            self.info('Candidate symbols_url: %s' % symbols_url)
            if not symbols_url.startswith('http'):
                return symbols_url

            try:
                # Let's see if the symbols are available
                urllib2.urlopen(symbols_url)
                return symbols_url

            except urllib2.HTTPError, e:
                self.warning("%s - %s" % (str(e), symbols_url))
                return None
        else:
            self.fatal("Can't figure out symbols_url from installer_url %s!" % installer_url)


    @PreScriptAction('run-tests')
    def _pre_run_tests(self, action):
        if self.releases is None and not (self.installer_url or self.installer_path):
            self.fatal('You need to call --determine-testing-configuration as well.')


    def _run_test(self, installer_path, symbols_url=None, update_channel=None, cleanup=True,
                  marionette_port=2828):
        '''
        All required steps for running the tests against an installer.
        '''
        dirs = self.query_abs_dirs()
        env = self.query_env(avoid_host_env=True)
        bin_dir = os.path.dirname(self.query_python_path())
        fx_ui_tests_bin = os.path.join(bin_dir, 'firefox-ui-update')
        gecko_log=os.path.join(dirs['abs_work_dir'], 'gecko.log')

        # Build the command
        cmd = [
            fx_ui_tests_bin,
            '--installer', installer_path,
            # Log to stdout until tests are stable.
            '--gecko-log=-',
            '--address=localhost:%s' % marionette_port,
        ]

        if symbols_url:
            cmd += ['--symbols-path', symbols_url]

        for arg in self.harness_extra_args:
            dest = arg[1]['dest']
            if dest in self.config:
                cmd += [' '.join(arg[0]), self.config[dest]]

        if update_channel:
            cmd += ['--update-channel', update_channel]

        return_code = self.run_command(cmd, cwd=dirs['abs_work_dir'],
                                       output_timeout=300,
                                       env=env)

        # Return more output if we fail
        if return_code != 0:
            self.info('Internally this is the command fx-ui-updates executed')
            self.info('%s' % ' '.join(map(str, cmd)))

            if os.path.exists(gecko_log):
                contents = self.read_from_file(gecko_log, verbose=False)
                self.warning('== Dumping gecko output ==')
                self.warning(contents)
                self.warning('== End of gecko output ==')
            else:
                # We're outputting to stdout with --gecko-log=- so there is not log to
                # complaing about. Remove the commented line below when changing
                # this behaviour.
                # self.warning('No gecko.log was found: %s' % gecko_log)
                pass

        if cleanup:
            for filepath in (installer_path, gecko_log):
                if os.path.exists(filepath):
                    self.debug('Removing %s' % filepath)
                    os.remove(filepath)

        return return_code


    def run_tests(self):
        dirs = self.query_abs_dirs()

        if self.installer_url or self.installer_path:
            if self.installer_url:
                self.installer_path = self.download_file(
                    self.installer_url,
                    parent_dir=dirs['abs_work_dir']
                )

            symbols_url = self._query_symbols_url(installer_url=self.installer_path)

            return self._run_test(
                installer_path=self.installer_path,
                symbols_url=symbols_url,
                cleanup=False
            )

        else:
            results = {}

            for rel_info in sorted(self.releases, key=lambda release: release['build_id']):
                build_id = rel_info['build_id']
                results[build_id] = {}

                self.info('About to run %s %s - %s locales' % (
                    build_id,
                    rel_info['from'],
                    len(rel_info['locales'])
                ))

                if self.config['dry_run']:
                    continue

                # Each locale gets a fresh port to avoid address in use errors in case of
                # tests that time out unexpectedly.
                marionette_port = 2827
                for locale in rel_info['locales']:
                    self.info("Running %s %s" % (build_id, locale))

                    # Safe temp hack to determine symbols URL from en-US build1 in the candidates dir
                    ftp_candidates_installer_url = self._modify_url(rel_info)
                    symbols_url = self._query_symbols_url(installer_url=ftp_candidates_installer_url)

                    # Determine from where to download the file
                    installer_url = '%s/%s' % (
                        rel_info['ftp_server_from'],
                        urllib.quote(rel_info['from'].replace('%locale%', locale))
                    )
                    installer_path = self.download_file(
                        url=installer_url,
                        parent_dir=dirs['abs_work_dir']
                    )

                    marionette_port += 1

                    retcode = self._run_test(
                        installer_path=installer_path,
                        symbols_url=symbols_url,
                        update_channel=self.channel,
                        marionette_port=marionette_port)

                    if retcode != 0:
                        self.warning('FAIL: firefox-ui-update has failed.' )

                        base_cmd = 'python scripts/firefox_ui_updates.py'
                        for c in self.config['config_files']:
                            base_cmd += ' --cfg %s' % c

                        base_cmd += ' --firefox-ui-branch %s --update-verify-config %s --tools-tag %s' % \
                            (self.firefox_ui_branch, self.update_verify_config, self.tools_tag)

                        base_cmd += ' --installer-url %s' % installer_url
                        if symbols_url:
                            base_cmd += ' --symbols-path %s' % symbols_url

                        self.info('You can run the *specific* locale on the same machine with:')
                        self.info('%s' % base_cmd)

                        self.info('You can run the *specific* locale on *your* machine with:')
                        self.info('%s --cfg developer_config.py' % base_cmd)

                    results[build_id][locale] = retcode

            # Determine which locales have failed and set scripts exit code
            exit_status = TBPL_SUCCESS
            for build_id in sorted(results.keys()):
                failed_locales = []
                for locale in sorted(results[build_id].keys()):
                    if results[build_id][locale] != 0:
                        failed_locales.append(locale)

                if failed_locales:
                    if exit_status == TBPL_SUCCESS:
                        self.info("")
                        self.info("SUMMARY - Firefox UI update tests failed locales:")
                        self.info("=================================================")
                        exit_status = TBPL_WARNING

                    self.info(build_id)
                    self.info("  %s" % (', '.join(failed_locales)))

            self.return_code = EXIT_STATUS_DICT[exit_status]


if __name__ == '__main__':
    myScript = FirefoxUIUpdates()
    myScript.run_and_exit()
