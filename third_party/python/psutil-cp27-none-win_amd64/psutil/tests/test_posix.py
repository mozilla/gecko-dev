#!/usr/bin/env python
# -*- coding: utf-8 -*-

# Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""POSIX specific tests."""

import datetime
import errno
import os
import re
import subprocess
import sys
import time

import psutil
from psutil import AIX
from psutil import BSD
from psutil import LINUX
from psutil import OPENBSD
from psutil import OSX
from psutil import POSIX
from psutil import SUNOS
from psutil._compat import callable
from psutil._compat import PY3
from psutil.tests import APPVEYOR
from psutil.tests import get_kernel_version
from psutil.tests import get_test_subprocess
from psutil.tests import mock
from psutil.tests import PYTHON_EXE
from psutil.tests import reap_children
from psutil.tests import retry_before_failing
from psutil.tests import run_test_module_by_name
from psutil.tests import sh
from psutil.tests import skip_on_access_denied
from psutil.tests import TRAVIS
from psutil.tests import unittest
from psutil.tests import wait_for_pid
from psutil.tests import which


def ps(cmd):
    """Expects a ps command with a -o argument and parse the result
    returning only the value of interest.
    """
    if not LINUX:
        cmd = cmd.replace(" --no-headers ", " ")
    if SUNOS:
        cmd = cmd.replace("-o start", "-o stime")
    if AIX:
        cmd = cmd.replace("-o rss", "-o rssize")
    output = sh(cmd)
    if not LINUX:
        output = output.split('\n')[1].strip()
    try:
        return int(output)
    except ValueError:
        return output

# ps "-o" field names differ wildly between platforms.
# "comm" means "only executable name" but is not available on BSD platforms.
# "args" means "command with all its arguments", and is also not available
# on BSD platforms.
# "command" is like "args" on most platforms, but like "comm" on AIX,
# and not available on SUNOS.
# so for the executable name we can use "comm" on Solaris and split "command"
# on other platforms.
# to get the cmdline (with args) we have to use "args" on AIX and
# Solaris, and can use "command" on all others.


def ps_name(pid):
    field = "command"
    if SUNOS:
        field = "comm"
    return ps("ps --no-headers -o %s -p %s" % (field, pid)).split(' ')[0]


def ps_args(pid):
    field = "command"
    if AIX or SUNOS:
        field = "args"
    return ps("ps --no-headers -o %s -p %s" % (field, pid))


@unittest.skipIf(not POSIX, "POSIX only")
class TestProcess(unittest.TestCase):
    """Compare psutil results against 'ps' command line utility (mainly)."""

    @classmethod
    def setUpClass(cls):
        cls.pid = get_test_subprocess([PYTHON_EXE, "-E", "-O"],
                                      stdin=subprocess.PIPE).pid
        wait_for_pid(cls.pid)

    @classmethod
    def tearDownClass(cls):
        reap_children()

    def test_ppid(self):
        ppid_ps = ps("ps --no-headers -o ppid -p %s" % self.pid)
        ppid_psutil = psutil.Process(self.pid).ppid()
        self.assertEqual(ppid_ps, ppid_psutil)

    def test_uid(self):
        uid_ps = ps("ps --no-headers -o uid -p %s" % self.pid)
        uid_psutil = psutil.Process(self.pid).uids().real
        self.assertEqual(uid_ps, uid_psutil)

    def test_gid(self):
        gid_ps = ps("ps --no-headers -o rgid -p %s" % self.pid)
        gid_psutil = psutil.Process(self.pid).gids().real
        self.assertEqual(gid_ps, gid_psutil)

    def test_username(self):
        username_ps = ps("ps --no-headers -o user -p %s" % self.pid)
        username_psutil = psutil.Process(self.pid).username()
        self.assertEqual(username_ps, username_psutil)

    def test_username_no_resolution(self):
        # Emulate a case where the system can't resolve the uid to
        # a username in which case psutil is supposed to return
        # the stringified uid.
        p = psutil.Process()
        with mock.patch("psutil.pwd.getpwuid", side_effect=KeyError) as fun:
            self.assertEqual(p.username(), str(p.uids().real))
            assert fun.called

    @skip_on_access_denied()
    @retry_before_failing()
    def test_rss_memory(self):
        # give python interpreter some time to properly initialize
        # so that the results are the same
        time.sleep(0.1)
        rss_ps = ps("ps --no-headers -o rss -p %s" % self.pid)
        rss_psutil = psutil.Process(self.pid).memory_info()[0] / 1024
        self.assertEqual(rss_ps, rss_psutil)

    @skip_on_access_denied()
    @retry_before_failing()
    def test_vsz_memory(self):
        # give python interpreter some time to properly initialize
        # so that the results are the same
        time.sleep(0.1)
        vsz_ps = ps("ps --no-headers -o vsz -p %s" % self.pid)
        vsz_psutil = psutil.Process(self.pid).memory_info()[1] / 1024
        self.assertEqual(vsz_ps, vsz_psutil)

    def test_name(self):
        name_ps = ps_name(self.pid)
        # remove path if there is any, from the command
        name_ps = os.path.basename(name_ps).lower()
        name_psutil = psutil.Process(self.pid).name().lower()
        # ...because of how we calculate PYTHON_EXE; on OSX this may
        # be "pythonX.Y".
        name_ps = re.sub(r"\d.\d", "", name_ps)
        name_psutil = re.sub(r"\d.\d", "", name_psutil)
        self.assertEqual(name_ps, name_psutil)

    def test_name_long(self):
        # On UNIX the kernel truncates the name to the first 15
        # characters. In such a case psutil tries to determine the
        # full name from the cmdline.
        name = "long-program-name"
        cmdline = ["long-program-name-extended", "foo", "bar"]
        with mock.patch("psutil._psplatform.Process.name",
                        return_value=name):
            with mock.patch("psutil._psplatform.Process.cmdline",
                            return_value=cmdline):
                p = psutil.Process()
                self.assertEqual(p.name(), "long-program-name-extended")

    def test_name_long_cmdline_ad_exc(self):
        # Same as above but emulates a case where cmdline() raises
        # AccessDenied in which case psutil is supposed to return
        # the truncated name instead of crashing.
        name = "long-program-name"
        with mock.patch("psutil._psplatform.Process.name",
                        return_value=name):
            with mock.patch("psutil._psplatform.Process.cmdline",
                            side_effect=psutil.AccessDenied(0, "")):
                p = psutil.Process()
                self.assertEqual(p.name(), "long-program-name")

    def test_name_long_cmdline_nsp_exc(self):
        # Same as above but emulates a case where cmdline() raises NSP
        # which is supposed to propagate.
        name = "long-program-name"
        with mock.patch("psutil._psplatform.Process.name",
                        return_value=name):
            with mock.patch("psutil._psplatform.Process.cmdline",
                            side_effect=psutil.NoSuchProcess(0, "")):
                p = psutil.Process()
                self.assertRaises(psutil.NoSuchProcess, p.name)

    @unittest.skipIf(OSX or BSD, 'ps -o start not available')
    def test_create_time(self):
        time_ps = ps("ps --no-headers -o start -p %s" % self.pid).split(' ')[0]
        time_psutil = psutil.Process(self.pid).create_time()
        time_psutil_tstamp = datetime.datetime.fromtimestamp(
            time_psutil).strftime("%H:%M:%S")
        # sometimes ps shows the time rounded up instead of down, so we check
        # for both possible values
        round_time_psutil = round(time_psutil)
        round_time_psutil_tstamp = datetime.datetime.fromtimestamp(
            round_time_psutil).strftime("%H:%M:%S")
        self.assertIn(time_ps, [time_psutil_tstamp, round_time_psutil_tstamp])

    def test_exe(self):
        ps_pathname = ps_name(self.pid)
        psutil_pathname = psutil.Process(self.pid).exe()
        try:
            self.assertEqual(ps_pathname, psutil_pathname)
        except AssertionError:
            # certain platforms such as BSD are more accurate returning:
            # "/usr/local/bin/python2.7"
            # ...instead of:
            # "/usr/local/bin/python"
            # We do not want to consider this difference in accuracy
            # an error.
            adjusted_ps_pathname = ps_pathname[:len(ps_pathname)]
            self.assertEqual(ps_pathname, adjusted_ps_pathname)

    def test_cmdline(self):
        ps_cmdline = ps_args(self.pid)
        psutil_cmdline = " ".join(psutil.Process(self.pid).cmdline())
        self.assertEqual(ps_cmdline, psutil_cmdline)

    # On SUNOS "ps" reads niceness /proc/pid/psinfo which returns an
    # incorrect value (20); the real deal is getpriority(2) which
    # returns 0; psutil relies on it, see:
    # https://github.com/giampaolo/psutil/issues/1082
    # AIX has the same issue
    @unittest.skipIf(SUNOS, "not reliable on SUNOS")
    @unittest.skipIf(AIX, "not reliable on AIX")
    def test_nice(self):
        ps_nice = ps("ps --no-headers -o nice -p %s" % self.pid)
        psutil_nice = psutil.Process().nice()
        self.assertEqual(ps_nice, psutil_nice)

    def test_num_fds(self):
        # Note: this fails from time to time; I'm keen on thinking
        # it doesn't mean something is broken
        def call(p, attr):
            args = ()
            attr = getattr(p, name, None)
            if attr is not None and callable(attr):
                if name == 'rlimit':
                    args = (psutil.RLIMIT_NOFILE,)
                attr(*args)
            else:
                attr

        p = psutil.Process(os.getpid())
        failures = []
        ignored_names = ['terminate', 'kill', 'suspend', 'resume', 'nice',
                         'send_signal', 'wait', 'children', 'as_dict',
                         'memory_info_ex']
        if LINUX and get_kernel_version() < (2, 6, 36):
            ignored_names.append('rlimit')
        if LINUX and get_kernel_version() < (2, 6, 23):
            ignored_names.append('num_ctx_switches')
        for name in dir(psutil.Process):
            if (name.startswith('_') or name in ignored_names):
                continue
            else:
                try:
                    num1 = p.num_fds()
                    for x in range(2):
                        call(p, name)
                    num2 = p.num_fds()
                except psutil.AccessDenied:
                    pass
                else:
                    if abs(num2 - num1) > 1:
                        fail = "failure while processing Process.%s method " \
                               "(before=%s, after=%s)" % (name, num1, num2)
                        failures.append(fail)
        if failures:
            self.fail('\n' + '\n'.join(failures))


@unittest.skipIf(not POSIX, "POSIX only")
class TestSystemAPIs(unittest.TestCase):
    """Test some system APIs."""

    @retry_before_failing()
    def test_pids(self):
        # Note: this test might fail if the OS is starting/killing
        # other processes in the meantime
        if SUNOS or AIX:
            cmd = ["ps", "-A", "-o", "pid"]
        else:
            cmd = ["ps", "ax", "-o", "pid"]
        p = get_test_subprocess(cmd, stdout=subprocess.PIPE)
        output = p.communicate()[0].strip()
        assert p.poll() == 0
        if PY3:
            output = str(output, sys.stdout.encoding)
        pids_ps = []
        for line in output.split('\n')[1:]:
            if line:
                pid = int(line.split()[0].strip())
                pids_ps.append(pid)
        # remove ps subprocess pid which is supposed to be dead in meantime
        pids_ps.remove(p.pid)
        pids_psutil = psutil.pids()
        pids_ps.sort()
        pids_psutil.sort()

        # on OSX and OPENBSD ps doesn't show pid 0
        if OSX or OPENBSD and 0 not in pids_ps:
            pids_ps.insert(0, 0)
        self.assertEqual(pids_ps, pids_psutil)

    # for some reason ifconfig -a does not report all interfaces
    # returned by psutil
    @unittest.skipIf(SUNOS, "unreliable on SUNOS")
    @unittest.skipIf(TRAVIS, "unreliable on TRAVIS")
    @unittest.skipIf(not which('ifconfig'), "no ifconfig cmd")
    def test_nic_names(self):
        output = sh("ifconfig -a")
        for nic in psutil.net_io_counters(pernic=True).keys():
            for line in output.split():
                if line.startswith(nic):
                    break
            else:
                self.fail(
                    "couldn't find %s nic in 'ifconfig -a' output\n%s" % (
                        nic, output))

    # can't find users on APPVEYOR or TRAVIS
    @unittest.skipIf(APPVEYOR or TRAVIS and not psutil.users(),
                     "unreliable on APPVEYOR or TRAVIS")
    @retry_before_failing()
    def test_users(self):
        out = sh("who")
        lines = out.split('\n')
        users = [x.split()[0] for x in lines]
        terminals = [x.split()[1] for x in lines]
        self.assertEqual(len(users), len(psutil.users()))
        for u in psutil.users():
            self.assertIn(u.name, users)
            self.assertIn(u.terminal, terminals)

    def test_pid_exists_let_raise(self):
        # According to "man 2 kill" possible error values for kill
        # are (EINVAL, EPERM, ESRCH). Test that any other errno
        # results in an exception.
        with mock.patch("psutil._psposix.os.kill",
                        side_effect=OSError(errno.EBADF, "")) as m:
            self.assertRaises(OSError, psutil._psposix.pid_exists, os.getpid())
            assert m.called

    def test_os_waitpid_let_raise(self):
        # os.waitpid() is supposed to catch EINTR and ECHILD only.
        # Test that any other errno results in an exception.
        with mock.patch("psutil._psposix.os.waitpid",
                        side_effect=OSError(errno.EBADF, "")) as m:
            self.assertRaises(OSError, psutil._psposix.wait_pid, os.getpid())
            assert m.called

    def test_os_waitpid_eintr(self):
        # os.waitpid() is supposed to "retry" on EINTR.
        with mock.patch("psutil._psposix.os.waitpid",
                        side_effect=OSError(errno.EINTR, "")) as m:
            self.assertRaises(
                psutil._psposix.TimeoutExpired,
                psutil._psposix.wait_pid, os.getpid(), timeout=0.01)
            assert m.called

    def test_os_waitpid_bad_ret_status(self):
        # Simulate os.waitpid() returning a bad status.
        with mock.patch("psutil._psposix.os.waitpid",
                        return_value=(1, -1)) as m:
            self.assertRaises(ValueError,
                              psutil._psposix.wait_pid, os.getpid())
            assert m.called

    # AIX can return '-' in df output instead of numbers, e.g. for /proc
    @unittest.skipIf(AIX, "unreliable on AIX")
    def test_disk_usage(self):
        def df(device):
            out = sh("df -k %s" % device).strip()
            line = out.split('\n')[1]
            fields = line.split()
            total = int(fields[1]) * 1024
            used = int(fields[2]) * 1024
            free = int(fields[3]) * 1024
            percent = float(fields[4].replace('%', ''))
            return (total, used, free, percent)

        tolerance = 4 * 1024 * 1024  # 4MB
        for part in psutil.disk_partitions(all=False):
            usage = psutil.disk_usage(part.mountpoint)
            try:
                total, used, free, percent = df(part.device)
            except RuntimeError as err:
                # see:
                # https://travis-ci.org/giampaolo/psutil/jobs/138338464
                # https://travis-ci.org/giampaolo/psutil/jobs/138343361
                err = str(err).lower()
                if "no such file or directory" in err or \
                        "raw devices not supported" in err or \
                        "permission denied" in err:
                    continue
                else:
                    raise
            else:
                self.assertAlmostEqual(usage.total, total, delta=tolerance)
                self.assertAlmostEqual(usage.used, used, delta=tolerance)
                self.assertAlmostEqual(usage.free, free, delta=tolerance)
                self.assertAlmostEqual(usage.percent, percent, delta=1)


if __name__ == '__main__':
    run_test_module_by_name(__file__)
