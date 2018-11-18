#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import json
import os
import subprocess
import sys
import time

import mozinfo

from mozdevice import ADBAndroid
from mozlog import commandline, get_default_logger
from mozprofile import create_profile
from mozrunner import runners

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))
if os.environ.get('SCRIPTSPATH', None) is not None:
    # in production it is env SCRIPTS_PATH
    mozharness_dir = os.environ['SCRIPTSPATH']
else:
    # locally it's in source tree
    mozharness_dir = os.path.join(here, '../../../mozharness')
sys.path.insert(0, mozharness_dir)

from mozharness.mozilla.firefox.autoconfig import _cfg_file_path, _autoconfig_path

webext_dir = os.path.join(os.path.dirname(here), 'webext')
sys.path.insert(0, here)

try:
    from mozbuild.base import MozbuildObject
    build = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build = None

from benchmark import Benchmark
from cmdline import parse_args
from control_server import RaptorControlServer
from gen_test_config import gen_test_config
from outputhandler import OutputHandler
from manifest import get_raptor_test_list
from playback import get_playback
from results import RaptorResultsHandler
from gecko_profile import GeckoProfile


def remove_autoconfig(binary):
    bindir = os.path.dirname(binary)
    mozillacfg = _cfg_file_path(bindir)
    if os.path.isfile(mozillacfg):
        os.unlink(mozillacfg)
    autoconfig = _autoconfig_path(bindir)
    if os.path.isfile(autoconfig):
        os.unlink(autoconfig)


class Raptor(object):
    """Container class for Raptor"""

    def __init__(self, app, binary, run_local=False, obj_path=None,
                 gecko_profile=False, gecko_profile_interval=None, gecko_profile_entries=None,
                 symbols_path=None, host=None, is_release_build=False, debug_mode=False):
        self.config = {}
        self.config['app'] = app
        self.config['binary'] = binary
        self.config['platform'] = mozinfo.os
        self.config['processor'] = mozinfo.processor
        self.config['run_local'] = run_local
        self.config['obj_path'] = obj_path
        self.config['gecko_profile'] = gecko_profile
        self.config['gecko_profile_interval'] = gecko_profile_interval
        self.config['gecko_profile_entries'] = gecko_profile_entries
        self.config['symbols_path'] = symbols_path
        self.config['host'] = host
        self.config['is_release_build'] = is_release_build
        self.raptor_venv = os.path.join(os.getcwd(), 'raptor-venv')
        self.log = get_default_logger(component='raptor-main')
        self.control_server = None
        self.playback = None
        self.benchmark = None
        self.gecko_profiler = None
        self.post_startup_delay = 30000

        # debug mode is currently only supported when running locally
        self.debug_mode = debug_mode if self.config['run_local'] else False

        # if running debug-mode reduce the pause after browser startup
        if self.debug_mode:
            self.post_startup_delay = 3000
            self.log.info("debug-mode enabled, reducing post-browser startup pause to %d ms"
                          % self.post_startup_delay)

        # Create the profile; for geckoview we want a firefox profile type
        if self.config['app'] == 'geckoview':
            self.profile = create_profile('firefox')
        else:
            self.profile = create_profile(self.config['app'])
            # Clear any existing mozilla.cfg file to prevent earlier
            # runs using mitmproxy from interfering with settings.
            remove_autoconfig(binary)

        # Merge in base profiles
        with open(os.path.join(self.profile_data_dir, 'profiles.json'), 'r') as fh:
            base_profiles = json.load(fh)['raptor']

        for name in base_profiles:
            path = os.path.join(self.profile_data_dir, name)
            self.log.info("Merging profile: {}".format(path))
            self.profile.merge(path)

        # create results holder
        self.results_handler = RaptorResultsHandler()

        # when testing desktop browsers we use mozrunner to start the browser; when
        # testing on android (i.e. geckoview) we use mozdevice to control the device app

        if self.config['app'] == "geckoview":
            # create the android device handler; it gets initiated and sets up adb etc
            self.log.info("creating android device handler using mozdevice")
            self.device = ADBAndroid(verbose=True)
            self.device.clear_logcat()
        else:
            # create the desktop browser runner
            self.log.info("creating browser runner using mozrunner")
            self.output_handler = OutputHandler()
            process_args = {
                'processOutputLine': [self.output_handler],
            }
            runner_cls = runners[app]
            self.runner = runner_cls(
                binary, profile=self.profile, process_args=process_args)

        self.log.info("raptor config: %s" % str(self.config))

    @property
    def profile_data_dir(self):
        if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
            return os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'], 'testing', 'profiles')
        if build:
            return os.path.join(build.topsrcdir, 'testing', 'profiles')
        return os.path.join(here, 'profile_data')

    def start_control_server(self):
        self.control_server = RaptorControlServer(self.results_handler, self.debug_mode)
        self.control_server.start()

        # for android we must make the control server available to the device
        if self.config['app'] == "geckoview" and self.config['host'] in ('localhost', '127.0.0.1'):
            self.log.info("making the raptor control server port available to device")
            _tcp_port = "tcp:%s" % self.control_server.port
            self.device.create_socket_connection('reverse', _tcp_port, _tcp_port)

    def get_playback_config(self, test):
        self.config['playback_tool'] = test.get('playback')
        self.log.info("test uses playback tool: %s " % self.config['playback_tool'])
        self.config['playback_binary_manifest'] = test.get('playback_binary_manifest', None)
        _key = 'playback_binary_zip_%s' % self.config['platform']
        self.config['playback_binary_zip'] = test.get(_key, None)
        self.config['playback_pageset_manifest'] = test.get('playback_pageset_manifest', None)
        _key = 'playback_pageset_zip_%s' % self.config['platform']
        self.config['playback_pageset_zip'] = test.get(_key, None)
        self.config['playback_recordings'] = test.get('playback_recordings', None)
        self.config['python3_win_manifest'] = test.get('python3_win_manifest', None)

    def run_test(self, test, timeout=None):
        self.log.info("starting raptor test: %s" % test['name'])
        self.log.info("test settings: %s" % str(test))
        self.log.info("raptor config: %s" % str(self.config))

        # benchmark-type tests require the benchmark test to be served out
        if test.get('type') == "benchmark":
            self.benchmark = Benchmark(self.config, test)
            benchmark_port = int(self.benchmark.port)
        else:
            benchmark_port = 0

        gen_test_config(self.config['app'],
                        test['name'],
                        self.control_server.port,
                        self.post_startup_delay,
                        host=self.config['host'],
                        b_port=benchmark_port,
                        debug_mode=1 if self.debug_mode else 0)

        # for android we must make the benchmarks server available to the device
        if self.config['app'] == "geckoview" and self.config['host'] in ('localhost', '127.0.0.1'):
            self.log.info("making the raptor benchmarks server port available to device")
            _tcp_port = "tcp:%s" % benchmark_port
            self.device.create_socket_connection('reverse', _tcp_port, _tcp_port)

        # must intall raptor addon each time because we dynamically update some content
        # note: for chrome the addon is just a list of paths that ultimately are added
        # to the chromium command line '--load-extension' argument
        raptor_webext = os.path.join(webext_dir, 'raptor')
        self.log.info("installing webext %s" % raptor_webext)
        self.profile.addons.install(raptor_webext)

        # add test specific preferences
        if test.get("preferences", None) is not None:
            if self.config['app'] == "firefox":
                self.profile.set_preferences(json.loads(test['preferences']))
            else:
                self.log.info("preferences were configured for the test, \
                              but we do not install them on non Firefox browsers.")

        # on firefox we can get an addon id; chrome addon actually is just cmd line arg
        if self.config['app'] in ["firefox", "geckoview"]:
            webext_id = self.profile.addons.addon_details(raptor_webext)['id']

        # some tests require tools to playback the test pages
        if test.get('playback', None) is not None:
            self.get_playback_config(test)
            # startup the playback tool
            self.playback = get_playback(self.config)

        if self.config['app'] in ("geckoview", "firefox") and \
           self.config['host'] not in ('localhost', '127.0.0.1'):
            # Must delete the proxy settings from the profile if running
            # the test with a host different from localhost.
            userjspath = os.path.join(self.profile.profile, 'user.js')
            with open(userjspath) as userjsfile:
                prefs = userjsfile.readlines()
            prefs = [pref for pref in prefs if 'network.proxy' not in pref]
            with open(userjspath, 'w') as userjsfile:
                userjsfile.writelines(prefs)

        # for geckoview we must copy the profile onto the device and set perms
        if self.config['app'] == "geckoview":
            if not self.device.is_app_installed(self.config['binary']):
                raise Exception('%s is not installed' % self.config['binary'])

            self.log.info("copying firefox profile onto the android device")
            self.device_profile = "/sdcard/raptor-profile"
            if self.device.is_dir(self.device_profile):
                self.device.rm(self.device_profile, recursive=True)
            self.device.mkdir(self.device_profile)
            self.device.push(self.profile.profile, self.device_profile)

            self.log.info("setting permisions to profile dir on the device")
            self.device.chmod(self.device_profile, recursive=True)

            # now start the geckoview app
            self.log.info("starting %s" % self.config['app'])

            extra_args = ["-profile", self.device_profile,
                          "--es", "env0", "LOG_VERBOSE=1",
                          "--es", "env1", "R_LOG_LEVEL=6"]

            try:
                # make sure the geckoview app is not running before
                # attempting to start.
                self.device.stop_application(self.config['binary'])
                self.device.launch_activity(self.config['binary'],
                                            "GeckoViewActivity",
                                            extra_args=extra_args,
                                            url='about:blank',
                                            e10s=True,
                                            fail_if_running=False)
            except Exception:
                self.log.error("Exception launching %s" % self.config['binary'])
                raise
            self.control_server.device = self.device
            self.control_server.app_name = self.config['binary']

        else:
            # For Firefox we need to set
            # MOZ_DISABLE_NONLOCAL_CONNECTIONS=1 env var before
            # startup when testing release builds from mozilla-beta or
            # mozilla-release. This is because of restrictions on
            # release builds that require webextensions to be signed
            # unless MOZ_DISABLE_NONLOCAL_CONNECTIONS is set to '1'.
            if self.config['app'] == "firefox" and self.config['is_release_build']:
                self.log.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=1")
                os.environ['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = "1"

            # if running debug-mode, tell Firefox to open the browser console on startup
            # for google chrome, open the devtools on the raptor test tab
            if self.debug_mode:
                if self.config['app'] == "firefox":
                    self.runner.cmdargs.extend(['-jsconsole'])
                if self.config['app'] == "chrome":
                    self.runner.cmdargs.extend(['--auto-open-devtools-for-tabs'])

            # now start the desktop browser
            self.log.info("starting %s" % self.config['app'])

            # if running a pageload test on google chrome, add the cmd line options
            # to turn on the proxy and ignore security certificate errors
            # if using host localhost, 127.0.0.1.
            if self.config['app'] == "chrome" and test.get('playback', None) is not None:
                chrome_args = [
                    '--proxy-server="http=127.0.0.1:8080;' +
                    'https=127.0.0.1:8080;ssl=127.0.0.1:8080"',
                    '--ignore-certificate-errors'
                ]
                if self.config['host'] not in ('localhost', '127.0.0.1'):
                    chrome_args[0] = chrome_args[0].replace('127.0.0.1', self.config['host'])
                if ' '.join(chrome_args) not in ' '.join(self.runner.cmdargs):
                    self.runner.cmdargs.extend(chrome_args)

            self.runner.start()
            proc = self.runner.process_handler
            self.output_handler.proc = proc

            self.control_server.browser_proc = proc

            # pageload tests need to be able to access non-local connections via mitmproxy
            if self.config['app'] == "firefox" and self.config['is_release_build'] and \
               test.get('playback', None) is not None:
                self.log.info("setting MOZ_DISABLE_NONLOCAL_CONNECTIONS=0")
                os.environ['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = "0"

            # if geckoProfile is enabled, initialize it
            if self.config['gecko_profile'] is True:
                self._init_gecko_profiling(test)
                # tell the control server the gecko_profile dir; the control server will
                # receive the actual gecko profiles from the web ext and will write them
                # to disk; then profiles are picked up by gecko_profile.symbolicate
                self.control_server.gecko_profile_dir = self.gecko_profiler.gecko_profile_dir

        # set our cs flag to indicate we are running the browser/app
        self.control_server._finished = False

        # convert to seconds and account for page cycles
        timeout = int(timeout / 1000) * int(test['page_cycles'])
        # account for the pause the raptor webext runner takes after browser startup
        timeout += int(self.post_startup_delay / 1000)

        # if geckoProfile enabled, give browser more time for profiling
        if self.config['gecko_profile'] is True:
            timeout += 5 * 60

        try:
            elapsed_time = 0
            while not self.control_server._finished:
                time.sleep(1)
                # we only want to force browser-shutdown on timeout if not in debug mode;
                # in debug-mode we leave the browser running (require manual shutdown)
                if not self.debug_mode:
                    elapsed_time += 1
                    if elapsed_time > (timeout) - 5:  # stop 5 seconds early
                        self.log.info("application timed out after {} seconds".format(timeout))
                        self.control_server.wait_for_quit()
                        break
        finally:
            if self.config['app'] != "geckoview":
                try:
                    self.runner.check_for_crashes()
                except NotImplementedError:  # not implemented for Chrome
                    pass
            # TODO: if on geckoview is there some cleanup here i.e. check for crashes?

        if self.playback is not None:
            self.playback.stop()

        # remove the raptor webext; as it must be reloaded with each subtest anyway
        self.log.info("removing webext %s" % raptor_webext)
        if self.config['app'] in ["firefox", "geckoview"]:
            self.profile.addons.remove_addon(webext_id)

        # for chrome the addon is just a list (appended to cmd line)
        if self.config['app'] in ["chrome", "chrome-android"]:
            self.profile.addons.remove(raptor_webext)

        # gecko profiling symbolication
        if self.config['gecko_profile'] is True:
            self.gecko_profiler.symbolicate()
            # clean up the temp gecko profiling folders
            self.log.info("cleaning up after gecko profiling")
            self.gecko_profiler.clean()

        # browser should be closed by now but this is a backup-shutdown (if not in debug-mode)
        if not self.debug_mode:
            if self.config['app'] != "geckoview":
                if self.runner.is_running():
                    self.runner.stop()
            # TODO the geckoview app should have been shutdown by this point by the
            # control server, but we can double-check here to make sure
        else:
            # in debug mode, and running locally, leave the browser running
            if self.config['run_local']:
                self.log.info("* debug-mode enabled - please shutdown the browser manually...")
                self.runner.wait(timeout=None)

    def _init_gecko_profiling(self, test):
        self.log.info("initializing gecko profiler")
        upload_dir = os.getenv('MOZ_UPLOAD_DIR')
        if not upload_dir:
            self.log.critical("Profiling ignored because MOZ_UPLOAD_DIR was not set")
        else:
            self.gecko_profiler = GeckoProfile(upload_dir,
                                               self.config,
                                               test)

    def process_results(self):
        # when running locally output results in build/raptor.json; when running
        # in production output to a local.json to be turned into tc job artifact
        if self.config.get('run_local', False):
            if 'MOZ_DEVELOPER_REPO_DIR' in os.environ:
                raptor_json_path = os.path.join(os.environ['MOZ_DEVELOPER_REPO_DIR'],
                                                'testing', 'mozharness', 'build', 'raptor.json')
            else:
                raptor_json_path = os.path.join(here, 'raptor.json')
        else:
            raptor_json_path = os.path.join(os.getcwd(), 'local.json')

        self.config['raptor_json_path'] = raptor_json_path
        return self.results_handler.summarize_and_output(self.config)

    def get_page_timeout_list(self):
        return self.results_handler.page_timeout_list

    def clean_up(self):
        self.control_server.stop()
        if self.config['app'] != "geckoview":
            self.runner.stop()
        elif self.config['app'] == 'geckoview':
            self.log.info('removing reverse socket connections')
            self.device.remove_socket_connections('reverse')
        else:
            pass
        remove_autoconfig(self.config['binary'])
        self.log.info("finished")


def view_gecko_profile(ffox_bin):
    # automatically load the latest talos gecko-profile archive in perf-html.io
    LOG = get_default_logger(component='raptor-view-gecko-profile')

    if sys.platform.startswith('win') and not ffox_bin.endswith(".exe"):
        ffox_bin = ffox_bin + ".exe"

    if not os.path.exists(ffox_bin):
        LOG.info("unable to find Firefox bin, cannot launch view-gecko-profile")
        return

    profile_zip = os.environ.get('RAPTOR_LATEST_GECKO_PROFILE_ARCHIVE', None)
    if profile_zip is None or not os.path.exists(profile_zip):
        LOG.info("No local talos gecko profiles were found so not launching perf-html.io")
        return

    # need the view-gecko-profile tool, it's in repo/testing/tools
    repo_dir = os.environ.get('MOZ_DEVELOPER_REPO_DIR', None)
    if repo_dir is None:
        LOG.info("unable to find MOZ_DEVELOPER_REPO_DIR, can't launch view-gecko-profile")
        return

    view_gp = os.path.join(repo_dir, 'testing', 'tools',
                           'view_gecko_profile', 'view_gecko_profile.py')
    if not os.path.exists(view_gp):
        LOG.info("unable to find the view-gecko-profile tool, cannot launch it")
        return

    command = ['python',
               view_gp,
               '-b', ffox_bin,
               '-p', profile_zip]

    LOG.info('Auto-loading this profile in perfhtml.io: %s' % profile_zip)
    LOG.info(command)

    # if the view-gecko-profile tool fails to launch for some reason, we don't
    # want to crash talos! just dump error and finsh up talos as usual
    try:
        view_profile = subprocess.Popen(command,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        # that will leave it running in own instance and let talos finish up
    except Exception as e:
        LOG.info("failed to launch view-gecko-profile tool, exeption: %s" % e)
        return

    time.sleep(5)
    ret = view_profile.poll()
    if ret is None:
        LOG.info("view-gecko-profile successfully started as pid %d" % view_profile.pid)
    else:
        LOG.error('view-gecko-profile process failed to start, poll returned: %s' % ret)


def main(args=sys.argv[1:]):
    args = parse_args()
    commandline.setup_logging('raptor', args, {'tbpl': sys.stdout})
    LOG = get_default_logger(component='raptor-main')

    LOG.info("raptor-start")

    if args.debug_mode:
        LOG.info("debug-mode enabled")

    LOG.info("received command line arguments: %s" % str(args))

    # if a test name specified on command line, and it exists, just run that one
    # otherwise run all available raptor tests that are found for this browser
    raptor_test_list = get_raptor_test_list(args, mozinfo.os)

    # ensure we have at least one valid test to run
    if len(raptor_test_list) == 0:
        LOG.critical("abort: no tests found")
        sys.exit(1)

    LOG.info("raptor tests scheduled to run:")
    for next_test in raptor_test_list:
        LOG.info(next_test['name'])

    raptor = Raptor(args.app,
                    args.binary,
                    run_local=args.run_local,
                    obj_path=args.obj_path,
                    gecko_profile=args.gecko_profile,
                    gecko_profile_interval=args.gecko_profile_interval,
                    gecko_profile_entries=args.gecko_profile_entries,
                    symbols_path=args.symbols_path,
                    host=args.host,
                    is_release_build=args.is_release_build,
                    debug_mode=args.debug_mode)

    raptor.start_control_server()

    for next_test in raptor_test_list:
        if 'page_timeout' not in next_test.keys():
            next_test['page_timeout'] = 120000
        if 'page_cycles' not in next_test.keys():
            next_test['page_cycles'] = 1

        raptor.run_test(next_test, timeout=int(next_test['page_timeout']))

    success = raptor.process_results()
    raptor.clean_up()

    if not success:
        # didn't get test results; test timed out or crashed, etc. we want job to fail
        LOG.critical("TEST-UNEXPECTED-FAIL: no raptor test results were found")
        os.sys.exit(1)

    # if we have results but one test page timed out (i.e. one tp6 test page didn't load
    # but others did) we still dumped PERFHERDER_DATA for the successfull pages but we
    # want the overall test job to marked as a failure
    pages_that_timed_out = raptor.get_page_timeout_list()
    if len(pages_that_timed_out) > 0:
        for _page in pages_that_timed_out:
            LOG.critical("TEST-UNEXPECTED-FAIL: test '%s' timed out loading test page: %s"
                         % (_page['test_name'], _page['url']))
        os.sys.exit(1)

    # when running raptor locally with gecko profiling on, use the view-gecko-profile
    # tool to automatically load the latest gecko profile in perf-html.io
    if args.gecko_profile and args.run_local:
        if os.environ.get('DISABLE_PROFILE_LAUNCH', '0') == '1':
            LOG.info("Not launching perf-html.io because DISABLE_PROFILE_LAUNCH=1")
        else:
            view_gecko_profile(args.binary)


if __name__ == "__main__":
    main()
