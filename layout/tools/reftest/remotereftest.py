# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import posixpath
import psutil
import signal
import sys
import tempfile
import time
import traceback
import urllib2
from contextlib import closing

from mozdevice import ADBAndroid, ADBTimeoutError
import mozinfo
from automation import Automation
from remoteautomation import RemoteAutomation, fennecLogcatFilters

from output import OutputHandler
from runreftest import RefTest, ReftestResolver
import reftestcommandline

# We need to know our current directory so that we can serve our test files from it.
SCRIPT_DIRECTORY = os.path.abspath(os.path.realpath(os.path.dirname(__file__)))


class RemoteReftestResolver(ReftestResolver):
    def absManifestPath(self, path):
        script_abs_path = os.path.join(SCRIPT_DIRECTORY, path)
        if os.path.exists(script_abs_path):
            rv = script_abs_path
        elif os.path.exists(os.path.abspath(path)):
            rv = os.path.abspath(path)
        else:
            print >> sys.stderr, "Could not find manifest %s" % script_abs_path
            sys.exit(1)
        return os.path.normpath(rv)

    def manifestURL(self, options, path):
        # Dynamically build the reftest URL if possible, beware that args[0] should exist 'inside'
        # webroot. It's possible for this url to have a leading "..", but reftest.js will fix that
        relPath = os.path.relpath(path, SCRIPT_DIRECTORY)
        return "http://%s:%s/%s" % (options.remoteWebServer, options.httpPort, relPath)


class ReftestServer:
    """ Web server used to serve Reftests, for closer fidelity to the real web.
        It is virtually identical to the server used in mochitest and will only
        be used for running reftests remotely.
        Bug 581257 has been filed to refactor this wrapper around httpd.js into
        it's own class and use it in both remote and non-remote testing. """

    def __init__(self, automation, options, scriptDir):
        self.automation = automation
        self.utilityPath = options.utilityPath
        self.xrePath = options.xrePath
        self.profileDir = options.serverProfilePath
        self.webServer = options.remoteWebServer
        self.httpPort = options.httpPort
        self.scriptDir = scriptDir
        self.httpdPath = os.path.abspath(options.httpdPath)
        if options.remoteWebServer == "10.0.2.2":
            # probably running an Android emulator and 10.0.2.2 will
            # not be visible from host
            shutdownServer = "127.0.0.1"
        else:
            shutdownServer = self.webServer
        self.shutdownURL = "http://%(server)s:%(port)s/server/shutdown" % {
                           "server": shutdownServer, "port": self.httpPort}

    def start(self):
        "Run the Refest server, returning the process ID of the server."

        env = self.automation.environment(xrePath=self.xrePath)
        env["XPCOM_DEBUG_BREAK"] = "warn"
        if self.automation.IS_WIN32:
            env["PATH"] = env["PATH"] + ";" + self.xrePath

        args = ["-g", self.xrePath,
                "-f", os.path.join(self.httpdPath, "httpd.js"),
                "-e", "const _PROFILE_PATH = '%(profile)s';const _SERVER_PORT = "
                      "'%(port)s'; const _SERVER_ADDR ='%(server)s';" % {
                      "profile": self.profileDir.replace('\\', '\\\\'), "port": self.httpPort,
                      "server": self.webServer},
                "-f", os.path.join(self.scriptDir, "server.js")]

        xpcshell = os.path.join(self.utilityPath,
                                "xpcshell" + self.automation.BIN_SUFFIX)

        if not os.access(xpcshell, os.F_OK):
            raise Exception('xpcshell not found at %s' % xpcshell)
        if self.automation.elf_arm(xpcshell):
            raise Exception('xpcshell at %s is an ARM binary; please use '
                            'the --utility-path argument to specify the path '
                            'to a desktop version.' % xpcshell)

        self._process = self.automation.Process([xpcshell] + args, env=env)
        pid = self._process.pid
        if pid < 0:
            print "TEST-UNEXPECTED-FAIL | remotereftests.py | Error starting server."
            return 2
        self.automation.log.info("INFO | remotereftests.py | Server pid: %d", pid)

    def ensureReady(self, timeout):
        assert timeout >= 0

        aliveFile = os.path.join(self.profileDir, "server_alive.txt")
        i = 0
        while i < timeout:
            if os.path.exists(aliveFile):
                break
            time.sleep(1)
            i += 1
        else:
            print ("TEST-UNEXPECTED-FAIL | remotereftests.py | "
                   "Timed out while waiting for server startup.")
            self.stop()
            return 1

    def stop(self):
        if hasattr(self, '_process'):
            try:
                with closing(urllib2.urlopen(self.shutdownURL)) as c:
                    c.read()

                rtncode = self._process.poll()
                if (rtncode is None):
                    self._process.terminate()
            except Exception:
                self.automation.log.info("Failed to shutdown server at %s" %
                                         self.shutdownURL)
                traceback.print_exc()
                self._process.kill()


class RemoteReftest(RefTest):
    use_marionette = False
    resolver_cls = RemoteReftestResolver

    def __init__(self, options, scriptDir):
        RefTest.__init__(self, options.suite)
        self.run_by_manifest = False
        self.scriptDir = scriptDir
        self.localLogName = options.localLogName

        verbose = False
        if options.log_tbpl_level == 'debug' or options.log_mach_level == 'debug':
            verbose = True
            print "set verbose!"
        self.device = ADBAndroid(adb=options.adb_path or 'adb',
                                 device=options.deviceSerial,
                                 test_root=options.remoteTestRoot,
                                 verbose=verbose)
        if options.remoteTestRoot is None:
            options.remoteTestRoot = posixpath.join(self.device.test_root, "reftest")
        options.remoteProfile = posixpath.join(options.remoteTestRoot, "profile")
        options.remoteLogFile = posixpath.join(options.remoteTestRoot, "reftest.log")
        options.logFile = options.remoteLogFile
        self.remoteProfile = options.remoteProfile
        self.remoteTestRoot = options.remoteTestRoot

        if not options.ignoreWindowSize:
            parts = self.device.get_info(
                'screen')['screen'][0].split()
            width = int(parts[0].split(':')[1])
            height = int(parts[1].split(':')[1])
            if (width < 1366 or height < 1050):
                self.error("ERROR: Invalid screen resolution %sx%s, "
                           "please adjust to 1366x1050 or higher" % (
                            width, height))

        self._populate_logger(options)
        self.outputHandler = OutputHandler(self.log, options.utilityPath, options.symbolsPath)
        # RemoteAutomation.py's 'messageLogger' is also used by mochitest. Mimic a mochitest
        # MessageLogger object to re-use this code path.
        self.outputHandler.write = self.outputHandler.__call__
        args = {'messageLogger': self.outputHandler}
        self.automation = RemoteAutomation(self.device,
                                           appName=options.app,
                                           remoteProfile=self.remoteProfile,
                                           remoteLog=options.remoteLogFile,
                                           processArgs=args)

        self.environment = self.automation.environment
        if self.automation.IS_DEBUG_BUILD:
            self.SERVER_STARTUP_TIMEOUT = 180
        else:
            self.SERVER_STARTUP_TIMEOUT = 90

        self.remoteCache = os.path.join(options.remoteTestRoot, "cache/")

        # Check that Firefox is installed
        expected = options.app.split('/')[-1]
        if not self.device.is_app_installed(expected):
            raise Exception("%s is not installed on this device" % expected)

        self.automation.deleteANRs()
        self.automation.deleteTombstones()
        self.device.clear_logcat()

        self.device.rm(self.remoteCache, force=True, recursive=True)

        procName = options.app.split('/')[-1]
        self.device.stop_application(procName)
        if self.device.process_exist(procName):
            self.log.error("unable to kill %s before starting tests!" % procName)

    def findPath(self, paths, filename=None):
        for path in paths:
            p = path
            if filename:
                p = os.path.join(p, filename)
            if os.path.exists(self.getFullPath(p)):
                return path
        return None

    def startWebServer(self, options):
        """ Create the webserver on the host and start it up """
        remoteXrePath = options.xrePath
        remoteUtilityPath = options.utilityPath
        localAutomation = Automation()
        localAutomation.IS_WIN32 = False
        localAutomation.IS_LINUX = False
        localAutomation.IS_MAC = False
        localAutomation.UNIXISH = False
        hostos = sys.platform
        if (hostos == 'mac' or hostos == 'darwin'):
            localAutomation.IS_MAC = True
        elif (hostos == 'linux' or hostos == 'linux2'):
            localAutomation.IS_LINUX = True
            localAutomation.UNIXISH = True
        elif (hostos == 'win32' or hostos == 'win64'):
            localAutomation.BIN_SUFFIX = ".exe"
            localAutomation.IS_WIN32 = True

        paths = [options.xrePath, localAutomation.DIST_BIN]
        options.xrePath = self.findPath(paths)
        if options.xrePath is None:
            print ("ERROR: unable to find xulrunner path for %s, "
                   "please specify with --xre-path" % (os.name))
            return 1
        paths.append("bin")
        paths.append(os.path.join("..", "bin"))

        xpcshell = "xpcshell"
        if (os.name == "nt"):
            xpcshell += ".exe"

        if (options.utilityPath):
            paths.insert(0, options.utilityPath)
        options.utilityPath = self.findPath(paths, xpcshell)
        if options.utilityPath is None:
            print ("ERROR: unable to find utility path for %s, "
                   "please specify with --utility-path" % (os.name))
            return 1

        options.serverProfilePath = tempfile.mkdtemp()
        self.server = ReftestServer(localAutomation, options, self.scriptDir)
        retVal = self.server.start()
        if retVal:
            return retVal
        retVal = self.server.ensureReady(self.SERVER_STARTUP_TIMEOUT)
        if retVal:
            return retVal

        options.xrePath = remoteXrePath
        options.utilityPath = remoteUtilityPath
        return 0

    def stopWebServer(self, options):
        self.server.stop()

    def killNamedProc(self, pname, orphans=True):
        """ Kill processes matching the given command name """
        self.log.info("Checking for %s processes..." % pname)

        for proc in psutil.process_iter():
            try:
                if proc.name() == pname:
                    procd = proc.as_dict(attrs=['pid', 'ppid', 'name', 'username'])
                    if proc.ppid() == 1 or not orphans:
                        self.log.info("killing %s" % procd)
                        try:
                            os.kill(proc.pid, getattr(signal, "SIGKILL", signal.SIGTERM))
                        except Exception as e:
                            self.log.info("Failed to kill process %d: %s" % (proc.pid, str(e)))
                    else:
                        self.log.info("NOT killing %s (not an orphan?)" % procd)
            except Exception:
                # may not be able to access process info for all processes
                continue

    def createReftestProfile(self, options, **kwargs):
        profile = RefTest.createReftestProfile(self,
                                               options,
                                               server=options.remoteWebServer,
                                               port=options.httpPort,
                                               **kwargs)
        profileDir = profile.profile
        prefs = {}
        prefs["app.update.url.android"] = ""
        prefs["browser.firstrun.show.localepicker"] = False
        prefs["reftest.remote"] = True
        prefs["datareporting.policy.dataSubmissionPolicyBypassAcceptance"] = True
        # move necko cache to a location that can be cleaned up
        prefs["browser.cache.disk.parent_directory"] = self.remoteCache

        prefs["layout.css.devPixelsPerPx"] = "1.0"
        # Because Fennec is a little wacky (see bug 1156817) we need to load the
        # reftest pages at 1.0 zoom, rather than zooming to fit the CSS viewport.
        prefs["apz.allow_zooming"] = False

        # Set the extra prefs.
        profile.set_preferences(prefs)

        try:
            self.device.push(profileDir, options.remoteProfile)
            self.device.chmod(options.remoteProfile, recursive=True, root=True)
        except Exception:
            print "Automation Error: Failed to copy profiledir to device"
            raise

        return profile

    def printDeviceInfo(self, printLogcat=False):
        try:
            if printLogcat:
                logcat = self.device.get_logcat(filter_out_regexps=fennecLogcatFilters)
                for l in logcat:
                    print "%s\n" % l.decode('utf-8', 'replace')
            print "Device info:"
            devinfo = self.device.get_info()
            for category in devinfo:
                if type(devinfo[category]) is list:
                    print "  %s:" % category
                    for item in devinfo[category]:
                        print "     %s" % item
                else:
                    print "  %s: %s" % (category, devinfo[category])
            print "Test root: %s" % self.device.test_root
        except ADBTimeoutError:
            raise
        except Exception as e:
            print "WARNING: Error getting device information: %s" % str(e)

    def environment(self, **kwargs):
        return self.automation.environment(**kwargs)

    def buildBrowserEnv(self, options, profileDir):
        browserEnv = RefTest.buildBrowserEnv(self, options, profileDir)
        # remove desktop environment not used on device
        if "XPCOM_MEM_BLOAT_LOG" in browserEnv:
            del browserEnv["XPCOM_MEM_BLOAT_LOG"]
        return browserEnv

    def runApp(self, options, cmdargs=None, timeout=None, debuggerInfo=None, symbolsPath=None,
               valgrindPath=None, valgrindArgs=None, valgrindSuppFiles=None, **profileArgs):
        if cmdargs is None:
            cmdargs = []

        if self.use_marionette:
            cmdargs.append('-marionette')

        binary = options.app
        profile = self.createReftestProfile(options, **profileArgs)

        # browser environment
        env = self.buildBrowserEnv(options, profile.profile)

        self.log.info("Running with e10s: {}".format(options.e10s))
        status, self.lastTestSeen = self.automation.runApp(None, env,
                                                           binary,
                                                           profile.profile,
                                                           cmdargs,
                                                           utilityPath=options.utilityPath,
                                                           xrePath=options.xrePath,
                                                           debuggerInfo=debuggerInfo,
                                                           symbolsPath=symbolsPath,
                                                           timeout=timeout,
                                                           e10s=options.e10s)

        self.cleanup(profile.profile)
        return status

    def cleanup(self, profileDir):
        self.device.rm(self.remoteTestRoot,  force=True, recursive=True)
        self.device.rm(self.remoteProfile, force=True, recursive=True)
        self.device.rm(self.remoteCache, force=True, recursive=True)
        RefTest.cleanup(self, profileDir)


def run_test_harness(parser, options):
    reftest = RemoteReftest(options, SCRIPT_DIRECTORY)
    parser.validate_remote(options, reftest.automation)
    parser.validate(options, reftest)

    if mozinfo.info['debug']:
        print "changing timeout for remote debug reftests from %s to 600 seconds" % options.timeout
        options.timeout = 600

    # Hack in a symbolic link for jsreftest
    os.system("ln -s ../jsreftest " + str(os.path.join(SCRIPT_DIRECTORY, "jsreftest")))

    # Despite our efforts to clean up servers started by this script, in practice
    # we still see infrequent cases where a process is orphaned and interferes
    # with future tests, typically because the old server is keeping the port in use.
    # Try to avoid those failures by checking for and killing servers before
    # trying to start new ones.
    reftest.killNamedProc('ssltunnel')
    reftest.killNamedProc('xpcshell')

    # Start the webserver
    retVal = reftest.startWebServer(options)
    if retVal:
        return retVal

    if options.printDeviceInfo and not options.verify:
        reftest.printDeviceInfo()

    retVal = 0
    try:
        if options.verify:
            retVal = reftest.verifyTests(options.tests, options)
        else:
            retVal = reftest.runTests(options.tests, options)
    except Exception:
        print "Automation Error: Exception caught while running tests"
        traceback.print_exc()
        retVal = 1

    reftest.stopWebServer(options)

    if options.printDeviceInfo and not options.verify:
        reftest.printDeviceInfo(printLogcat=True)

    return retVal


if __name__ == "__main__":
    parser = reftestcommandline.RemoteArgumentsParser()
    options = parser.parse_args()
    sys.exit(run_test_harness(parser, options))
