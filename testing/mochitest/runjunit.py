# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import os
import posixpath
import re
import shutil
import sys
import tempfile
import traceback

import mozcrash
import mozinfo
import mozlog
import moznetwork
from mozdevice import ADBAndroid, ADBError
from mozprofile import Profile, DEFAULT_PORTS
from mozprofile.permissions import ServerLocations
from runtests import MochitestDesktop, update_mozinfo

here = os.path.abspath(os.path.dirname(__file__))

try:
    from mozbuild.base import (
        MozbuildObject,
        MachCommandConditions as conditions,
    )
    build_obj = MozbuildObject.from_environment(cwd=here)
except ImportError:
    build_obj = None
    conditions = None


class JUnitTestRunner(MochitestDesktop):
    """
       A test harness to run geckoview junit tests on a remote device.
    """

    def __init__(self, log, options):
        self.log = log
        verbose = False
        if options.log_tbpl_level == 'debug' or options.log_mach_level == 'debug':
            verbose = True
        self.device = ADBAndroid(adb=options.adbPath or 'adb',
                                 device=options.deviceSerial,
                                 test_root=options.remoteTestRoot,
                                 verbose=verbose)
        self.options = options
        self.log.debug("options=%s" % vars(options))
        update_mozinfo()
        self.remote_profile = posixpath.join(self.device.test_root, 'junit-profile')

        if self.options.coverage and not self.options.coverage_output_dir:
            raise Exception("--coverage-output-dir is required when using --enable-coverage")
        if self.options.coverage:
            self.remote_coverage_output_file = posixpath.join(self.device.test_root,
                                                              'junit-coverage.ec')
            self.coverage_output_file = os.path.join(self.options.coverage_output_dir,
                                                     'junit-coverage.ec')

        self.server_init()

        self.cleanup()
        self.device.clear_logcat()
        self.build_profile()
        self.startServers(
            self.options,
            debuggerInfo=None,
            ignoreSSLTunnelExts=True)
        self.log.debug("Servers started")

    def server_init(self):
        """
           Additional initialization required to satisfy MochitestDesktop.startServers
        """
        self._locations = None
        self.server = None
        self.wsserver = None
        self.websocketProcessBridge = None
        self.SERVER_STARTUP_TIMEOUT = 180 if mozinfo.info.get('debug') else 90
        if self.options.remoteWebServer is None:
            if os.name != "nt":
                self.options.remoteWebServer = moznetwork.get_ip()
            else:
                raise Exception("--remote-webserver must be specified")
        self.options.webServer = self.options.remoteWebServer
        self.options.webSocketPort = '9988'
        self.options.httpdPath = None
        self.options.keep_open = False
        self.options.pidFile = ""
        self.options.subsuite = None
        self.options.xrePath = None
        if build_obj and 'MOZ_HOST_BIN' in os.environ:
            self.options.xrePath = os.environ['MOZ_HOST_BIN']
            if not self.options.utilityPath:
                self.options.utilityPath = self.options.xrePath
        if not self.options.xrePath:
            self.options.xrePath = self.options.utilityPath
        if build_obj:
            self.options.certPath = os.path.join(build_obj.topsrcdir,
                                                 'build', 'pgo', 'certs')

    def build_profile(self):
        """
           Create a local profile with test prefs and proxy definitions and
           push it to the remote device.
        """

        self.profile = Profile(locations=self.locations, proxy=self.proxy(self.options))
        self.options.profilePath = self.profile.profile

        # Set preferences
        self.merge_base_profiles(self.options)

        if self.fillCertificateDB(self.options):
            self.log.error("Certificate integration failed")

        self.device.mkdir(self.remote_profile, parents=True)
        self.device.push(self.profile.profile, self.remote_profile)
        self.log.debug("profile %s -> %s" %
                       (str(self.profile.profile), str(self.remote_profile)))

    def cleanup(self):
        try:
            self.stopServers()
            self.log.debug("Servers stopped")
            self.device.stop_application(self.options.app)
            self.device.rm(self.remote_profile, force=True, recursive=True)
            if hasattr(self, 'profile'):
                del self.profile
        except Exception:
            traceback.print_exc()
            self.log.info("Caught and ignored an exception during cleanup")

    def build_command_line(self, test_filters):
        """
           Construct and return the 'am instrument' command line.
        """
        cmd = "am instrument -w -r"
        # profile location
        cmd = cmd + " -e args '-profile %s'" % self.remote_profile
        # multi-process
        e10s = 'true' if self.options.e10s else 'false'
        cmd = cmd + " -e use_multiprocess %s" % e10s
        # chunks (shards)
        shards = self.options.totalChunks
        shard = self.options.thisChunk
        if shards is not None and shard is not None:
            shard -= 1  # shard index is 0 based
            cmd = cmd + " -e numShards %d -e shardIndex %d" % (shards, shard)
        # test filters: limit run to specific test(s)
        for f in test_filters:
            # filter can be class-name or 'class-name#method-name' (single test)
            cmd = cmd + " -e class %s" % f
        # enable code coverage reports
        if self.options.coverage:
            cmd = cmd + " -e coverage true"
            cmd = cmd + " -e coverageFile %s" % self.remote_coverage_output_file
        # environment
        env = {}
        env["MOZ_CRASHREPORTER"] = "1"
        env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        env["MOZ_CRASHREPORTER_SHUTDOWN"] = "1"
        env["XPCOM_DEBUG_BREAK"] = "stack"
        env["DISABLE_UNSAFE_CPOW_WARNINGS"] = "1"
        env["MOZ_DISABLE_NONLOCAL_CONNECTIONS"] = "1"
        env["MOZ_IN_AUTOMATION"] = "1"
        env["R_LOG_VERBOSE"] = "1"
        env["R_LOG_LEVEL"] = "6"
        env["R_LOG_DESTINATION"] = "stderr"
        for (env_count, (env_key, env_val)) in enumerate(env.iteritems()):
            cmd = cmd + " -e env%d %s=%s" % (env_count, env_key, env_val)
        # runner
        cmd = cmd + " %s/%s" % (self.options.app, self.options.runner)
        return cmd

    @property
    def locations(self):
        if self._locations is not None:
            return self._locations
        locations_file = os.path.join(here, 'server-locations.txt')
        self._locations = ServerLocations(locations_file)
        return self._locations

    def run_tests(self, test_filters=None):
        """
           Run the tests.
        """
        if not self.device.is_app_installed(self.options.app):
            raise Exception("%s is not installed" %
                            self.options.app)
        if self.device.process_exist(self.options.app):
            raise Exception("%s already running before starting tests" %
                            self.options.app)

        self.test_started = False
        self.pass_count = 0
        self.fail_count = 0
        self.todo_count = 0
        self.class_name = ""
        self.test_name = ""
        self.current_full_name = ""

        def callback(line):
            # Output callback: Parse the raw junit log messages, translating into
            # treeherder-friendly test start/pass/fail messages.

            self.log.process_output(self.options.app, str(line))
            # Expect per-test info like: "INSTRUMENTATION_STATUS: class=something"
            match = re.match(r'INSTRUMENTATION_STATUS:\s*class=(.*)', line)
            if match:
                self.class_name = match.group(1)
            # Expect per-test info like: "INSTRUMENTATION_STATUS: test=something"
            match = re.match(r'INSTRUMENTATION_STATUS:\s*test=(.*)', line)
            if match:
                self.test_name = match.group(1)
            # Expect per-test info like: "INSTRUMENTATION_STATUS_CODE: 0|1|..."
            match = re.match(r'INSTRUMENTATION_STATUS_CODE:\s*([+-]?\d+)', line)
            if match:
                status = match.group(1)
                full_name = "%s.%s" % (self.class_name, self.test_name)
                if full_name == self.current_full_name:
                    if status == '0':
                        message = ''
                        status = 'PASS'
                        expected = 'PASS'
                        self.pass_count += 1
                    elif status == '-3':  # ignored (skipped)
                        message = ''
                        status = 'SKIP'
                        expected = 'SKIP'
                        self.todo_count += 1
                    elif status == '-4':  # known fail
                        message = ''
                        status = 'FAIL'
                        expected = 'FAIL'
                        self.todo_count += 1
                    else:
                        message = 'status %s' % status
                        status = 'FAIL'
                        expected = 'PASS'
                        self.fail_count += 1
                    self.log.test_end(full_name, status, expected, message)
                    self.test_started = False
                else:
                    if self.test_started:
                        # next test started without reporting previous status
                        self.fail_count += 1
                        status = 'FAIL'
                        expected = 'PASS'
                        self.log.test_end(self.current_full_name, status, expected,
                                          "missing test completion status")
                    self.log.test_start(full_name)
                    self.test_started = True
                    self.current_full_name = full_name

        # Ideally all test names should be reported to suite_start, but these test
        # names are not known in advance.
        self.log.suite_start(["geckoview-junit"])
        try:
            self.device.grant_runtime_permissions(self.options.app)
            cmd = self.build_command_line(test_filters)
            self.log.info("launching %s" % cmd)
            p = self.device.shell(cmd, timeout=self.options.max_time, stdout_callback=callback)
            if p.timedout:
                self.log.error("TEST-UNEXPECTED-TIMEOUT | runjunit.py | "
                               "Timed out after %d seconds" % self.options.max_time)
            self.log.info("Passed: %d" % self.pass_count)
            self.log.info("Failed: %d" % self.fail_count)
            self.log.info("Todo: %d" % self.todo_count)
        finally:
            self.log.suite_end()

        if self.check_for_crashes():
            self.fail_count = 1

        if self.options.coverage:
            try:
                self.device.pull(self.remote_coverage_output_file,
                                 self.coverage_output_file)
            except ADBError:
                # Avoid a task retry in case the code coverage file is not found.
                self.log.error("No code coverage file (%s) found on remote device" %
                               self.remote_coverage_output_file)
                return -1

        return 1 if self.fail_count else 0

    def check_for_crashes(self):
        logcat = self.device.get_logcat()
        if logcat:
            if mozcrash.check_for_java_exception(logcat, self.current_full_name):
                return True
        symbols_path = self.options.symbolsPath
        try:
            dump_dir = tempfile.mkdtemp()
            remote_dir = posixpath.join(self.remote_profile, 'minidumps')
            if not self.device.is_dir(remote_dir):
                # If crash reporting is enabled (MOZ_CRASHREPORTER=1), the
                # minidumps directory is automatically created when the app
                # (first) starts, so its lack of presence is a hint that
                # something went wrong.
                print "Automation Error: No crash directory (%s) found on remote device" % \
                    remote_dir
                return True
            self.device.pull(remote_dir, dump_dir)
            crashed = mozcrash.log_crashes(self.log, dump_dir, symbols_path,
                                           test=self.current_full_name)
        finally:
            try:
                shutil.rmtree(dump_dir)
            except Exception:
                self.log.warning("unable to remove directory: %s" % dump_dir)
        return crashed


class JunitArgumentParser(argparse.ArgumentParser):
    """
       An argument parser for geckoview-junit.
    """
    def __init__(self, **kwargs):
        super(JunitArgumentParser, self).__init__(**kwargs)

        self.add_argument("--appname",
                          action="store",
                          type=str,
                          dest="app",
                          default="org.mozilla.geckoview.test",
                          help="Test package name.")
        self.add_argument("--adbpath",
                          action="store",
                          type=str,
                          dest="adbPath",
                          default=None,
                          help="Path to adb executable.")
        self.add_argument("--deviceSerial",
                          action="store",
                          type=str,
                          dest="deviceSerial",
                          help="adb serial number of remote device.")
        self.add_argument("--remoteTestRoot",
                          action="store",
                          type=str,
                          dest="remoteTestRoot",
                          help="Remote directory to use as test root "
                               "(eg. /mnt/sdcard/tests or /data/local/tests).")
        self.add_argument("--disable-e10s",
                          action="store_false",
                          dest="e10s",
                          default=True,
                          help="Disable multiprocess mode in test app.")
        self.add_argument("--max-time",
                          action="store",
                          type=int,
                          dest="max_time",
                          default="2400",
                          help="Max time in seconds to wait for tests (default 2400s).")
        self.add_argument("--runner",
                          action="store",
                          type=str,
                          dest="runner",
                          default="android.support.test.runner.AndroidJUnitRunner",
                          help="Test runner name.")
        self.add_argument("--symbols-path",
                          action="store",
                          type=str,
                          dest="symbolsPath",
                          default=None,
                          help="Path to directory containing breakpad symbols, "
                               "or the URL of a zip file containing symbols.")
        self.add_argument("--utility-path",
                          action="store",
                          type=str,
                          dest="utilityPath",
                          default=None,
                          help="Path to directory containing host utility programs.")
        self.add_argument("--total-chunks",
                          action="store",
                          type=int,
                          dest="totalChunks",
                          default=None,
                          help="Total number of chunks to split tests into.")
        self.add_argument("--this-chunk",
                          action="store",
                          type=int,
                          dest="thisChunk",
                          default=None,
                          help="If running tests by chunks, the chunk number to run.")
        self.add_argument("--enable-coverage",
                          action="store_true",
                          dest="coverage",
                          default=False,
                          help="Enable code coverage collection.")
        self.add_argument("--coverage-output-dir",
                          action="store",
                          type=str,
                          dest="coverage_output_dir",
                          default=None,
                          help="If collecting code coverage, save the report file in this dir.")
        # Additional options for server.
        self.add_argument("--certificate-path",
                          action="store",
                          type=str,
                          dest="certPath",
                          default=None,
                          help="Path to directory containing certificate store."),
        self.add_argument("--http-port",
                          action="store",
                          type=str,
                          dest="httpPort",
                          default=DEFAULT_PORTS['http'],
                          help="Port of the web server for http traffic.")
        self.add_argument("--remote-webserver",
                          action="store",
                          type=str,
                          dest="remoteWebServer",
                          help="IP address of the webserver.")
        self.add_argument("--ssl-port",
                          action="store",
                          type=str,
                          dest="sslPort",
                          default=DEFAULT_PORTS['https'],
                          help="Port of the web server for https traffic.")
        # Remaining arguments are test filters.
        self.add_argument("test_filters",
                          nargs="*",
                          help="Test filter(s): class and/or method names of test(s) to run.")

        mozlog.commandline.add_logging_group(self)


def run_test_harness(parser, options):
    if hasattr(options, 'log'):
        log = options.log
    else:
        log = mozlog.commandline.setup_logging("runjunit", options,
                                               {"tbpl": sys.stdout})
    runner = JUnitTestRunner(log, options)
    result = -1
    try:
        result = runner.run_tests(options.test_filters)
    except KeyboardInterrupt:
        log.info("runjunit.py | Received keyboard interrupt")
        result = -1
    except Exception:
        traceback.print_exc()
        log.error(
            "runjunit.py | Received unexpected exception while running tests")
        result = 1
    finally:
        runner.cleanup()
    return result


def main(args=sys.argv[1:]):
    parser = JunitArgumentParser()
    options = parser.parse_args()
    return run_test_harness(parser, options)


if __name__ == "__main__":
    sys.exit(main())
