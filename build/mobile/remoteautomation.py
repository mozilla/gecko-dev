# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import datetime
import glob
import time
import re
import os
import posixpath
import tempfile
import shutil
import sys

from automation import Automation
from mozdevice import ADBTimeoutError
from mozlog import get_default_logger
from mozscreenshot import dump_screen, dump_device_screen
import mozcrash

# signatures for logcat messages that we don't care about much
fennecLogcatFilters = ["The character encoding of the HTML document was not declared",
                       "Use of Mutation Events is deprecated. Use MutationObserver instead.",
                       "Unexpected value from nativeGetEnabledTags: 0"]


class RemoteAutomation(Automation):

    def __init__(self, device, appName='', remoteProfile=None, remoteLog=None,
                 processArgs=None):
        self.device = device
        self.appName = appName
        self.remoteProfile = remoteProfile
        self.remoteLog = remoteLog
        self.processArgs = processArgs or {}
        self.lastTestSeen = "remoteautomation.py"
        Automation.__init__(self)

    def runApp(self, testURL, env, app, profileDir, extraArgs,
               utilityPath=None, xrePath=None, debuggerInfo=None, symbolsPath=None,
               timeout=-1, maxTime=None, e10s=True, **kwargs):
        """
        Run the app, log the duration it took to execute, return the status code.
        Kills the app if it runs for longer than |maxTime| seconds, or outputs nothing
        for |timeout| seconds.
        """

        if utilityPath is None:
            utilityPath = self.DIST_BIN
        if xrePath is None:
            xrePath = self.DIST_BIN
        if timeout == -1:
            timeout = self.DEFAULT_TIMEOUT
        self.utilityPath = utilityPath

        cmd, args = self.buildCommandLine(app, debuggerInfo, profileDir, testURL, extraArgs)
        startTime = datetime.datetime.now()

        self.lastTestSeen = "remoteautomation.py"
        self.launchApp([cmd] + args,
                       env=self.environment(env=env, crashreporter=not debuggerInfo),
                       e10s=e10s, **self.processArgs)

        self.log.info("remoteautomation.py | Application pid: %d", self.pid)

        status = self.waitForFinish(timeout, maxTime)
        self.log.info("remoteautomation.py | Application ran for: %s",
                      str(datetime.datetime.now() - startTime))

        crashed = self.checkForCrashes(symbolsPath)
        if crashed:
            status = 1

        return status, self.lastTestSeen

    # Set up what we need for the remote environment
    def environment(self, env=None, crashreporter=True, **kwargs):
        # Because we are running remote, we don't want to mimic the local env
        # so no copying of os.environ
        if env is None:
            env = {}

        if crashreporter:
            env['MOZ_CRASHREPORTER_NO_REPORT'] = '1'
            env['MOZ_CRASHREPORTER'] = '1'
            env['MOZ_CRASHREPORTER_SHUTDOWN'] = '1'
        else:
            env['MOZ_CRASHREPORTER_DISABLE'] = '1'

        # Crash on non-local network connections by default.
        # MOZ_DISABLE_NONLOCAL_CONNECTIONS can be set to "0" to temporarily
        # enable non-local connections for the purposes of local testing.
        # Don't override the user's choice here.  See bug 1049688.
        env.setdefault('MOZ_DISABLE_NONLOCAL_CONNECTIONS', '1')

        # Send an env var noting that we are in automation. Passing any
        # value except the empty string will declare the value to exist.
        #
        # This may be used to disabled network connections during testing, e.g.
        # Switchboard & telemetry uploads.
        env.setdefault('MOZ_IN_AUTOMATION', '1')

        # Set WebRTC logging in case it is not set yet.
        # On Android, environment variables cannot contain ',' so the
        # standard WebRTC setting for NSPR_LOG_MODULES is not available.
        # env.setdefault('NSPR_LOG_MODULES', 'signaling:5,mtransport:5,datachannel:5,jsep:5,MediaPipelineFactory:5')  # NOQA: E501
        env.setdefault('R_LOG_LEVEL', '6')
        env.setdefault('R_LOG_DESTINATION', 'stderr')
        env.setdefault('R_LOG_VERBOSE', '1')

        return env

    def waitForFinish(self, timeout, maxTime):
        """ Wait for tests to finish.
            If maxTime seconds elapse or no output is detected for timeout
            seconds, kill the process and fail the test.
        """
        # maxTime is used to override the default timeout, we should honor that
        status = self.wait(timeout=maxTime, noOutputTimeout=timeout)

        topActivity = self.device.get_top_activity(timeout=60)
        if topActivity == self.procName:
            self.log.info("%s unexpectedly found running. Killing..." % self.procName)
            self.kill(True)
        if status == 1:
            if maxTime:
                self.log.error("TEST-UNEXPECTED-FAIL | %s | "
                               "application ran for longer than allowed maximum time "
                               "of %s seconds" % (self.lastTestSeen, maxTime))
            else:
                self.log.error("TEST-UNEXPECTED-FAIL | %s | "
                               "application ran for longer than allowed maximum time"
                               % self.lastTestSeen)
        if status == 2:
            self.log.error("TEST-UNEXPECTED-FAIL | %s | "
                           "application timed out after %d seconds with no output"
                           % (self.lastTestSeen, int(timeout)))

        return status

    def deleteANRs(self):
        # Remove files from the dalvik stack-trace directory.
        if not self.device.is_dir(self.device.stack_trace_dir, root=True):
            return
        try:
            for trace_file in self.device.ls(self.device.stack_trace_dir, root=True):
                trace_path = posixpath.join(self.device.stack_trace_dir, trace_file)
                self.device.chmod(trace_path, root=True)
                self.device.rm(trace_path, root=True)
        except Exception as e:
            print("Error deleting %s: %s" % (self.device.stack_trace_dir, str(e)))

    def checkForANRs(self):
        if not self.device.is_dir(self.device.stack_trace_dir):
            print("%s not found" % self.device.stack_trace_dir)
            return
        try:
            for trace_file in self.device.ls(self.device.stack_trace_dir, root=True):
                trace_path = posixpath.join(self.device.stack_trace_dir, trace_file)
                t = self.device.get_file(trace_path)
                if t:
                    stripped = t.strip()
                    if len(stripped) > 0:
                        print("Contents of %s:" % trace_path)
                        print(t)
            # Once reported, delete traces
            self.deleteANRs()
        except Exception as e:
            print("Error pulling %s: %s" % (self.device.stack_trace_dir, str(e)))

    def deleteTombstones(self):
        # delete any tombstone files from device
        self.device.rm("/data/tombstones", force=True, recursive=True, root=True)

    def checkForTombstones(self):
        # pull any tombstones from device and move to MOZ_UPLOAD_DIR
        remoteDir = "/data/tombstones"
        uploadDir = os.environ.get('MOZ_UPLOAD_DIR', None)
        if uploadDir:
            if not os.path.exists(uploadDir):
                os.mkdir(uploadDir)
            if self.device.is_dir(remoteDir):
                # copy tombstone files from device to local upload directory
                self.device.chmod(remoteDir, recursive=True, root=True)
                self.device.pull(remoteDir, uploadDir)
                self.deleteTombstones()
                for f in glob.glob(os.path.join(uploadDir, "tombstone_??")):
                    # add a unique integer to the file name, in case there are
                    # multiple tombstones generated with the same name, for
                    # instance, after multiple robocop tests
                    for i in xrange(1, sys.maxint):
                        newname = "%s.%d.txt" % (f, i)
                        if not os.path.exists(newname):
                            os.rename(f, newname)
                            break
            else:
                print("%s does not exist; tombstone check skipped" % remoteDir)
        else:
            print("MOZ_UPLOAD_DIR not defined; tombstone check skipped")

    def checkForCrashes(self, symbolsPath):
        self.checkForANRs()
        self.checkForTombstones()

        logcat = self.device.get_logcat(
            filter_out_regexps=fennecLogcatFilters)

        javaException = mozcrash.check_for_java_exception(
            logcat, test_name=self.lastTestSeen)
        if javaException:
            return True

        # If crash reporting is disabled (MOZ_CRASHREPORTER!=1), we can't say
        # anything.
        if not self.CRASHREPORTER:
            return False

        try:
            dumpDir = tempfile.mkdtemp()
            remoteCrashDir = posixpath.join(self.remoteProfile, 'minidumps')
            if not self.device.is_dir(remoteCrashDir):
                # If crash reporting is enabled (MOZ_CRASHREPORTER=1), the
                # minidumps directory is automatically created when Fennec
                # (first) starts, so its lack of presence is a hint that
                # something went wrong.
                print("Automation Error: No crash directory (%s) found on remote device" %
                      remoteCrashDir)
                return True
            self.device.pull(remoteCrashDir, dumpDir)

            logger = get_default_logger()
            crashed = mozcrash.log_crashes(
                logger, dumpDir, symbolsPath, test=self.lastTestSeen)

        finally:
            try:
                shutil.rmtree(dumpDir)
            except Exception as e:
                print("WARNING: unable to remove directory %s: %s" % (
                    dumpDir, str(e)))
        return crashed

    def buildCommandLine(self, app, debuggerInfo, profileDir, testURL, extraArgs):
        # If remote profile is specified, use that instead
        if self.remoteProfile:
            profileDir = self.remoteProfile

        # Hack for robocop, if app is "am" and extraArgs contains the rest of the stuff, lets
        # assume extraArgs is all we need
        if app == "am" and extraArgs[0] in ('instrument', 'start'):
            return app, extraArgs

        cmd, args = Automation.buildCommandLine(
            self, app, debuggerInfo, profileDir, testURL, extraArgs)
        try:
            args.remove('-foreground')
        except Exception:
            pass
        return app, args

    def launchApp(self, cmd, env=None, e10s=True, messageLogger=None, counts=None):
        self.messageLogger = messageLogger
        self.stdoutlen = 0

        if self.appName and self.device.process_exist(self.appName):
            print("remoteautomation.py %s is already running. Stopping..." % self.appName)
            self.device.stop_application(self.appName, root=True)

        self.counts = counts
        if self.counts is not None:
            self.counts['pass'] = 0
            self.counts['fail'] = 0
            self.counts['todo'] = 0

        if cmd[0] == 'am':
            cmd = ' '.join(cmd)
            self.procName = self.appName
            if not self.device.shell_bool(cmd):
                print("remote_automation.py failed to launch %s" % cmd)
        else:
            self.procName = cmd[0].split(posixpath.sep)[-1]
            args = cmd
            if args[0] == self.appName:
                args = args[1:]
            url = args[-1:][0]
            if url.startswith('/'):
                # this is probably a reftest profile directory, not a url
                url = None
            else:
                args = args[:-1]
            if 'geckoview' in self.appName:
                activity = "TestRunnerActivity"
                self.device.launch_activity(self.appName, activity_name=activity, e10s=e10s,
                                            moz_env=env, extra_args=args, url=url)
            else:
                self.device.launch_fennec(self.appName, moz_env=env, extra_args=args, url=url)

        # Setting timeout at 1 hour since on a remote device this takes much longer.
        # Temporarily increased to 110 minutes because no more chunks can be created.
        self.timeout = 6600

        # Used to buffer log messages until we meet a line break
        self.logBuffer = ""

    @property
    def pid(self):
        procs = self.device.get_process_list()
        # limit the comparison to the first 75 characters due to a
        # limitation in processname length in android.
        pids = [proc[0] for proc in procs if proc[1] == self.procName[:75]]

        if pids is None or len(pids) < 1:
            return 0
        return pids[0]

    def read_stdout(self):
        """
        Fetch the full remote log file, log any new content and return True if new
        content processed.
        """
        if not self.device.is_file(self.remoteLog):
            return False
        try:
            newLogContent = self.device.get_file(self.remoteLog, offset=self.stdoutlen)
        except ADBTimeoutError:
            raise
        except Exception as e:
            self.log.info("remoteautomation.py | exception reading log: %s" % str(e))
            return False
        if not newLogContent:
            return False

        self.stdoutlen += len(newLogContent)

        if self.messageLogger is None:
            testStartFilenames = re.findall(r"TEST-START \| ([^\s]*)", newLogContent)
            if testStartFilenames:
                self.lastTestSeen = testStartFilenames[-1]
            print(newLogContent)
            return True

        self.logBuffer += newLogContent
        lines = self.logBuffer.split('\n')
        lines = [l for l in lines if l]

        if lines:
            if self.logBuffer.endswith('\n'):
                # all lines are complete; no need to buffer
                self.logBuffer = ""
            else:
                # keep the last (unfinished) line in the buffer
                self.logBuffer = lines[-1]
                del lines[-1]

        if not lines:
            return False

        for line in lines:
            # This passes the line to the logger (to be logged or buffered)
            parsed_messages = self.messageLogger.write(line)
            for message in parsed_messages:
                if isinstance(message, dict) and message.get('action') == 'test_start':
                    self.lastTestSeen = message['test']
                if isinstance(message, dict) and message.get('action') == 'log':
                    line = message['message'].strip()
                    if self.counts:
                        m = re.match(".*:\s*(\d*)", line)
                        if m:
                            try:
                                val = int(m.group(1))
                                if "Passed:" in line:
                                    self.counts['pass'] += val
                                elif "Failed:" in line:
                                    self.counts['fail'] += val
                                elif "Todo:" in line:
                                    self.counts['todo'] += val
                            except ADBTimeoutError:
                                raise
                            except Exception:
                                pass

        return True

    # Wait for the remote process to end (or for its activity to go to background).
    # While waiting, periodically retrieve the process output and print it.
    # If the process is still running after *timeout* seconds, return 1;
    # If the process is still running but no output is received in *noOutputTimeout*
    # seconds, return 2;
    # Else, once the process exits/goes to background, return 0.
    def wait(self, timeout=None, noOutputTimeout=None):
        timer = 0
        noOutputTimer = 0
        interval = 10
        if timeout is None:
            timeout = self.timeout
        status = 0
        top = self.procName
        slowLog = False
        endTime = datetime.datetime.now() + datetime.timedelta(seconds=timeout)
        while top == self.procName:
            # Get log updates on each interval, but if it is taking
            # too long, only do it every 60 seconds
            hasOutput = False
            if (not slowLog) or (timer % 60 == 0):
                startRead = datetime.datetime.now()
                hasOutput = self.read_stdout()
                if (datetime.datetime.now() - startRead) > datetime.timedelta(seconds=5):
                    slowLog = True
                if hasOutput:
                    noOutputTimer = 0
                if self.counts and 'pass' in self.counts and self.counts['pass'] > 0:
                    interval = 0.5
            time.sleep(interval)
            timer += interval
            noOutputTimer += interval
            if datetime.datetime.now() > endTime:
                status = 1
                break
            if (noOutputTimeout and noOutputTimer > noOutputTimeout):
                status = 2
                break
            if not hasOutput:
                top = self.device.get_top_activity(timeout=60)
                if top is None:
                    print("Failed to get top activity, retrying, once...")
                    top = self.device.get_top_activity(timeout=60)
        # Flush anything added to stdout during the sleep
        self.read_stdout()
        print("wait for %s complete; top activity=%s" % (self.procName, top))
        return status

    def kill(self, stagedShutdown=False):
        # Take a screenshot to capture the screen state just before
        # the application is killed.
        # Do not use the on-device screenshot options since
        # they rarely work well with Firefox on the Android
        # emulator. dump_screen provides an effective
        # screenshot of the emulator and its host desktop.
        if not self.device._device_serial.startswith('emulator-'):
            dump_device_screen(self.device, get_default_logger())
        elif self.utilityPath:
            dump_screen(self.utilityPath, get_default_logger())
        if stagedShutdown:
            # Trigger an ANR report with "kill -3" (SIGQUIT)
            try:
                self.device.pkill(self.procName, sig=3, attempts=1, root=True)
            except ADBTimeoutError:
                raise
            except:  # NOQA: E722
                pass
            time.sleep(3)
            # Trigger a breakpad dump with "kill -6" (SIGABRT)
            try:
                self.device.pkill(self.procName, sig=6, attempts=1, root=True)
            except ADBTimeoutError:
                raise
            except:  # NOQA: E722
                pass
            # Wait for process to end
            retries = 0
            while retries < 3:
                if self.device.process_exist(self.procName):
                    print("%s still alive after SIGABRT: waiting..." % self.procName)
                    time.sleep(5)
                else:
                    break
                retries += 1
            if self.device.process_exist(self.procName):
                try:
                    self.device.pkill(self.procName, sig=9, attempts=1, root=True)
                except ADBTimeoutError:
                    raise
                except:  # NOQA: E722
                    print("%s still alive after SIGKILL!" % self.procName)
            if self.device.process_exist(self.procName):
                self.device.stop_application(self.procName)
        else:
            self.device.stop_application(self.procName)
        # Test harnesses use the MOZ_CRASHREPORTER environment variables to suppress
        # the interactive crash reporter, but that may not always be effective;
        # check for and cleanup errant crashreporters.
        crashreporter = "%s.CrashReporter" % self.procName
        if self.device.process_exist(crashreporter):
            print("Warning: %s unexpectedly found running. Killing..." % crashreporter)
            try:
                self.device.pkill(crashreporter, root=True)
            except ADBTimeoutError:
                raise
            except:  # NOQA: E722
                pass
        if self.device.process_exist(crashreporter):
            print("ERROR: %s still running!!" % crashreporter)
