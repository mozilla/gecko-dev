#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import with_statement
import sys, os, tempfile, shutil
from optparse import OptionParser
import mozprocess, mozinfo, mozlog, mozcrash, mozfile
from contextlib import contextmanager

log = mozlog.getLogger('cppunittests')

class CPPUnitTests(object):
    # Time (seconds) to wait for test process to complete
    TEST_PROC_TIMEOUT = 900
    # Time (seconds) in which process will be killed if it produces no output.
    TEST_PROC_NO_OUTPUT_TIMEOUT = 300

    def run_one_test(self, prog, env, symbols_path=None):
        """
        Run a single C++ unit test program.

        Arguments:
        * prog: The path to the test program to run.
        * env: The environment to use for running the program.
        * symbols_path: A path to a directory containing Breakpad-formatted
                        symbol files for producing stack traces on crash.

        Return True if the program exits with a zero status, False otherwise.
        """
        basename = os.path.basename(prog)
        log.info("Running test %s", basename)
        with mozfile.TemporaryDirectory() as tempdir:
            proc = mozprocess.ProcessHandler([prog],
                                             cwd=tempdir,
                                             env=env)
            #TODO: After bug 811320 is fixed, don't let .run() kill the process,
            # instead use a timeout in .wait() and then kill to get a stack.
            proc.run(timeout=CPPUnitTests.TEST_PROC_TIMEOUT,
                     outputTimeout=CPPUnitTests.TEST_PROC_NO_OUTPUT_TIMEOUT)
            proc.wait()
            if proc.timedOut:
                log.testFail("%s | timed out after %d seconds",
                             basename, CPPUnitTests.TEST_PROC_TIMEOUT)
                return False
            if mozcrash.check_for_crashes(tempdir, symbols_path,
                                          test_name=basename):
                log.testFail("%s | test crashed", basename)
                return False
            result = proc.proc.returncode == 0
            if not result:
                log.testFail("%s | test failed with return code %d",
                             basename, proc.proc.returncode)
            return result

    def build_core_environment(self, env = {}):
        """
        Add environment variables likely to be used across all platforms, including remote systems.
        """
        env["MOZILLA_FIVE_HOME"] = self.xre_path
        env["MOZ_XRE_DIR"] = self.xre_path
        #TODO: switch this to just abort once all C++ unit tests have
        # been fixed to enable crash reporting
        env["XPCOM_DEBUG_BREAK"] = "stack-and-abort"
        env["MOZ_CRASHREPORTER_NO_REPORT"] = "1"
        env["MOZ_CRASHREPORTER"] = "1"
        return env

    def build_environment(self):
        """
        Create and return a dictionary of all the appropriate env variables and values.
        On a remote system, we overload this to set different values and are missing things like os.environ and PATH.
        """
        if not os.path.isdir(self.xre_path):
            raise Exception("xre_path does not exist: %s", self.xre_path)
        env = dict(os.environ)
        env = self.build_core_environment(env)
        pathvar = ""
        if mozinfo.os == "linux":
            pathvar = "LD_LIBRARY_PATH"
        elif mozinfo.os == "mac":
            pathvar = "DYLD_LIBRARY_PATH"
        elif mozinfo.os == "win":
            pathvar = "PATH"
        if pathvar:
            if pathvar in env:
                env[pathvar] = "%s%s%s" % (self.xre_path, os.pathsep, env[pathvar])
            else:
                env[pathvar] = self.xre_path

        # Use llvm-symbolizer for ASan if available/required
        llvmsym = os.path.join(self.xre_path, "llvm-symbolizer")
        if os.path.isfile(llvmsym):
          env["ASAN_SYMBOLIZER_PATH"] = llvmsym
          log.info("INFO | runcppunittests.py | ASan using symbolizer at %s", llvmsym)
        else:
          log.info("TEST-UNEXPECTED-FAIL | runcppunittests.py | Failed to find ASan symbolizer at %s", llvmsym)

        return env

    def run_tests(self, programs, xre_path, symbols_path=None):
        """
        Run a set of C++ unit test programs.

        Arguments:
        * programs: An iterable containing paths to test programs.
        * xre_path: A path to a directory containing a XUL Runtime Environment.
        * symbols_path: A path to a directory containing Breakpad-formatted
                        symbol files for producing stack traces on crash.

        Returns True if all test programs exited with a zero status, False
        otherwise.
        """
        self.xre_path = xre_path
        env = self.build_environment()
        pass_count = 0
        fail_count = 0
        for prog in programs:
            single_result = self.run_one_test(prog, env, symbols_path)
            if single_result:
                pass_count += 1
            else:
                fail_count += 1

        log.info("Result summary:")
        log.info("Passed: %d" % pass_count)
        log.info("Failed: %d" % fail_count)
        return fail_count == 0

class CPPUnittestOptions(OptionParser):
    def __init__(self):
        OptionParser.__init__(self)
        self.add_option("--xre-path",
                        action = "store", type = "string", dest = "xre_path",
                        default = None,
                        help = "absolute path to directory containing XRE (probably xulrunner)")
        self.add_option("--symbols-path",
                        action = "store", type = "string", dest = "symbols_path",
                        default = None,
                        help = "absolute path to directory containing breakpad symbols, or the URL of a zip file containing symbols")
        self.add_option("--skip-manifest",
                        action = "store", type = "string", dest = "manifest_file",
                        default = None,
                        help = "absolute path to a manifest file")

def extract_unittests_from_args(args, manifest_file):
    """Extract unittests from args, expanding directories as needed"""
    progs = []

    # Known files commonly packaged with the cppunittests that are not tests
    skipped_progs = set(['.mkdir.done', 'remotecppunittests.py', 'runcppunittests.py', 'runcppunittests.pyc'])

    if manifest_file:
        skipped_progs.add(os.path.basename(manifest_file))
        with open(manifest_file) as f:
            for line in f:
                # strip out comment, if any
                prog = line.split('#')[0]
                if prog:
                    skipped_progs.add(prog.strip())

    for p in args:
        if os.path.isdir(p):
            progs.extend([os.path.abspath(os.path.join(p, x)) for x in os.listdir(p) if not x in skipped_progs])
        elif p not in skipped_progs:
            progs.append(os.path.abspath(p))

    return progs

def main():
    parser = CPPUnittestOptions()
    options, args = parser.parse_args()
    if not args:
        print >>sys.stderr, """Usage: %s <test binary> [<test binary>...]""" % sys.argv[0]
        sys.exit(1)
    if not options.xre_path:
        print >>sys.stderr, """Error: --xre-path is required"""
        sys.exit(1)
        
    progs = extract_unittests_from_args(args, options.manifest_file)
    options.xre_path = os.path.abspath(options.xre_path)
    tester = CPPUnitTests()
    try:
        result = tester.run_tests(progs, options.xre_path, options.symbols_path)
    except Exception, e:
        log.error(str(e))
        result = False
    sys.exit(0 if result else 1)

if __name__ == '__main__':
    main()

