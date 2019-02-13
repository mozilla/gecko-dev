#!/usr/bin/env python

# Copyright (c) 2009, Giampaolo Rodola'. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Windows specific tests.  These are implicitly run by test_psutil.py."""

import errno
import os
import platform
import signal
import subprocess
import sys
import time
import traceback

from test_psutil import (get_test_subprocess, reap_children, unittest)

try:
    import wmi
except ImportError:
    wmi = None
try:
    import win32api
    import win32con
except ImportError:
    win32api = win32con = None

from psutil._compat import PY3, callable, long
from psutil._pswindows import ACCESS_DENIED_SET
import _psutil_windows
import psutil


def wrap_exceptions(fun):
    def wrapper(self, *args, **kwargs):
        try:
            return fun(self, *args, **kwargs)
        except OSError:
            err = sys.exc_info()[1]
            if err.errno in ACCESS_DENIED_SET:
                raise psutil.AccessDenied(None, None)
            if err.errno == errno.ESRCH:
                raise psutil.NoSuchProcess(None, None)
            raise
    return wrapper


class WindowsSpecificTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.pid = get_test_subprocess().pid

    @classmethod
    def tearDownClass(cls):
        reap_children()

    def test_issue_24(self):
        p = psutil.Process(0)
        self.assertRaises(psutil.AccessDenied, p.kill)

    def test_special_pid(self):
        p = psutil.Process(4)
        self.assertEqual(p.name(), 'System')
        # use __str__ to access all common Process properties to check
        # that nothing strange happens
        str(p)
        p.username()
        self.assertTrue(p.create_time() >= 0.0)
        try:
            rss, vms = p.memory_info()
        except psutil.AccessDenied:
            # expected on Windows Vista and Windows 7
            if not platform.uname()[1] in ('vista', 'win-7', 'win7'):
                raise
        else:
            self.assertTrue(rss > 0)

    def test_send_signal(self):
        p = psutil.Process(self.pid)
        self.assertRaises(ValueError, p.send_signal, signal.SIGINT)

    def test_nic_names(self):
        p = subprocess.Popen(['ipconfig', '/all'], stdout=subprocess.PIPE)
        out = p.communicate()[0]
        if PY3:
            out = str(out, sys.stdout.encoding)
        nics = psutil.net_io_counters(pernic=True).keys()
        for nic in nics:
            if "pseudo-interface" in nic.replace(' ', '-').lower():
                continue
            if nic not in out:
                self.fail(
                    "%r nic wasn't found in 'ipconfig /all' output" % nic)

    def test_exe(self):
        for p in psutil.process_iter():
            try:
                self.assertEqual(os.path.basename(p.exe()), p.name())
            except psutil.Error:
                pass

    # --- Process class tests

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_process_name(self):
        w = wmi.WMI().Win32_Process(ProcessId=self.pid)[0]
        p = psutil.Process(self.pid)
        self.assertEqual(p.name(), w.Caption)

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_process_exe(self):
        w = wmi.WMI().Win32_Process(ProcessId=self.pid)[0]
        p = psutil.Process(self.pid)
        self.assertEqual(p.exe(), w.ExecutablePath)

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_process_cmdline(self):
        w = wmi.WMI().Win32_Process(ProcessId=self.pid)[0]
        p = psutil.Process(self.pid)
        self.assertEqual(' '.join(p.cmdline()),
                         w.CommandLine.replace('"', ''))

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_process_username(self):
        w = wmi.WMI().Win32_Process(ProcessId=self.pid)[0]
        p = psutil.Process(self.pid)
        domain, _, username = w.GetOwner()
        username = "%s\\%s" % (domain, username)
        self.assertEqual(p.username(), username)

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_process_rss_memory(self):
        time.sleep(0.1)
        w = wmi.WMI().Win32_Process(ProcessId=self.pid)[0]
        p = psutil.Process(self.pid)
        rss = p.memory_info().rss
        self.assertEqual(rss, int(w.WorkingSetSize))

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_process_vms_memory(self):
        time.sleep(0.1)
        w = wmi.WMI().Win32_Process(ProcessId=self.pid)[0]
        p = psutil.Process(self.pid)
        vms = p.memory_info().vms
        # http://msdn.microsoft.com/en-us/library/aa394372(VS.85).aspx
        # ...claims that PageFileUsage is represented in Kilo
        # bytes but funnily enough on certain platforms bytes are
        # returned instead.
        wmi_usage = int(w.PageFileUsage)
        if (vms != wmi_usage) and (vms != wmi_usage * 1024):
            self.fail("wmi=%s, psutil=%s" % (wmi_usage, vms))

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_process_create_time(self):
        w = wmi.WMI().Win32_Process(ProcessId=self.pid)[0]
        p = psutil.Process(self.pid)
        wmic_create = str(w.CreationDate.split('.')[0])
        psutil_create = time.strftime("%Y%m%d%H%M%S",
                                      time.localtime(p.create_time()))
        self.assertEqual(wmic_create, psutil_create)

    # --- psutil namespace functions and constants tests

    @unittest.skipUnless(hasattr(os, 'NUMBER_OF_PROCESSORS'),
                         'NUMBER_OF_PROCESSORS env var is not available')
    def test_cpu_count(self):
        num_cpus = int(os.environ['NUMBER_OF_PROCESSORS'])
        self.assertEqual(num_cpus, psutil.cpu_count())

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_total_phymem(self):
        w = wmi.WMI().Win32_ComputerSystem()[0]
        self.assertEqual(int(w.TotalPhysicalMemory),
                         psutil.virtual_memory().total)

    # @unittest.skipIf(wmi is None, "wmi module is not installed")
    # def test__UPTIME(self):
    #     # _UPTIME constant is not public but it is used internally
    #     # as value to return for pid 0 creation time.
    #     # WMI behaves the same.
    #     w = wmi.WMI().Win32_Process(ProcessId=self.pid)[0]
    #     p = psutil.Process(0)
    #     wmic_create = str(w.CreationDate.split('.')[0])
    #     psutil_create = time.strftime("%Y%m%d%H%M%S",
    #                                   time.localtime(p.create_time()))
    #

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_pids(self):
        # Note: this test might fail if the OS is starting/killing
        # other processes in the meantime
        w = wmi.WMI().Win32_Process()
        wmi_pids = [x.ProcessId for x in w]
        wmi_pids.sort()
        psutil_pids = psutil.pids()
        psutil_pids.sort()
        if wmi_pids != psutil_pids:
            difference = \
                filter(lambda x: x not in wmi_pids, psutil_pids) + \
                filter(lambda x: x not in psutil_pids, wmi_pids)
            self.fail("difference: " + str(difference))

    @unittest.skipIf(wmi is None, "wmi module is not installed")
    def test_disks(self):
        ps_parts = psutil.disk_partitions(all=True)
        wmi_parts = wmi.WMI().Win32_LogicalDisk()
        for ps_part in ps_parts:
            for wmi_part in wmi_parts:
                if ps_part.device.replace('\\', '') == wmi_part.DeviceID:
                    if not ps_part.mountpoint:
                        # this is usually a CD-ROM with no disk inserted
                        break
                    try:
                        usage = psutil.disk_usage(ps_part.mountpoint)
                    except OSError:
                        err = sys.exc_info()[1]
                        if err.errno == errno.ENOENT:
                            # usually this is the floppy
                            break
                        else:
                            raise
                    self.assertEqual(usage.total, int(wmi_part.Size))
                    wmi_free = int(wmi_part.FreeSpace)
                    self.assertEqual(usage.free, wmi_free)
                    # 10 MB tollerance
                    if abs(usage.free - wmi_free) > 10 * 1024 * 1024:
                        self.fail("psutil=%s, wmi=%s" % (
                            usage.free, wmi_free))
                    break
            else:
                self.fail("can't find partition %s" % repr(ps_part))

    @unittest.skipIf(win32api is None, "pywin32 module is not installed")
    def test_num_handles(self):
        p = psutil.Process(os.getpid())
        before = p.num_handles()
        handle = win32api.OpenProcess(win32con.PROCESS_QUERY_INFORMATION,
                                      win32con.FALSE, os.getpid())
        after = p.num_handles()
        self.assertEqual(after, before + 1)
        win32api.CloseHandle(handle)
        self.assertEqual(p.num_handles(), before)

    @unittest.skipIf(win32api is None, "pywin32 module is not installed")
    def test_num_handles_2(self):
        # Note: this fails from time to time; I'm keen on thinking
        # it doesn't mean something is broken
        def call(p, attr):
            attr = getattr(p, name, None)
            if attr is not None and callable(attr):
                attr()
            else:
                attr

        p = psutil.Process(self.pid)
        failures = []
        for name in dir(psutil.Process):
            if name.startswith('_') \
                or name.startswith('set_') \
                or name.startswith('get')  \
                or name in ('terminate', 'kill', 'suspend', 'resume',
                            'nice', 'send_signal', 'wait', 'children',
                            'as_dict'):
                continue
            else:
                try:
                    call(p, name)
                    num1 = p.num_handles()
                    call(p, name)
                    num2 = p.num_handles()
                except (psutil.NoSuchProcess, psutil.AccessDenied):
                    pass
                else:
                    if num2 > num1:
                        fail = \
                            "failure while processing Process.%s method " \
                            "(before=%s, after=%s)" % (name, num1, num2)
                        failures.append(fail)
        if failures:
            self.fail('\n' + '\n'.join(failures))


class TestDualProcessImplementation(unittest.TestCase):
    fun_names = [
        # function name, tolerance
        ('proc_cpu_times', 0.2),
        ('proc_create_time', 0.5),
        ('proc_num_handles', 1),  # 1 because impl #1 opens a handle
        ('proc_io_counters', 0),
        ('proc_memory_info', 1024),  # KB
    ]

    def test_compare_values(self):
        # Certain APIs on Windows have 2 internal implementations, one
        # based on documented Windows APIs, another one based
        # NtQuerySystemInformation() which gets called as fallback in
        # case the first fails because of limited permission error.
        # Here we test that the two methods return the exact same value,
        # see:
        # https://github.com/giampaolo/psutil/issues/304
        def assert_ge_0(obj):
            if isinstance(obj, tuple):
                for value in obj:
                    self.assertGreaterEqual(value, 0)
            elif isinstance(obj, (int, long, float)):
                self.assertGreaterEqual(obj, 0)
            else:
                assert 0  # case not handled which needs to be fixed

        def compare_with_tolerance(ret1, ret2, tolerance):
            if ret1 == ret2:
                return
            else:
                if isinstance(ret2, (int, long, float)):
                    diff = abs(ret1 - ret2)
                    self.assertLessEqual(diff, tolerance)
                elif isinstance(ret2, tuple):
                    for a, b in zip(ret1, ret2):
                        diff = abs(a - b)
                        self.assertLessEqual(diff, tolerance)

        failures = []
        for name, tolerance in self.fun_names:
            meth1 = wrap_exceptions(getattr(_psutil_windows, name))
            meth2 = wrap_exceptions(getattr(_psutil_windows, name + '_2'))
            for p in psutil.process_iter():
                if name == 'proc_memory_info' and p.pid == os.getpid():
                    continue
                #
                try:
                    ret1 = meth1(p.pid)
                except psutil.NoSuchProcess:
                    continue
                except psutil.AccessDenied:
                    ret1 = None
                #
                try:
                    ret2 = meth2(p.pid)
                except psutil.NoSuchProcess:
                    # this is supposed to fail only in case of zombie process
                    # never for permission error
                    continue

                # compare values
                try:
                    if ret1 is None:
                        assert_ge_0(ret2)
                    else:
                        compare_with_tolerance(ret1, ret2, tolerance)
                        assert_ge_0(ret1)
                        assert_ge_0(ret2)
                except AssertionError:
                    trace = traceback.format_exc()
                    msg = '%s\npid=%s, method=%r, ret_1=%r, ret_2=%r' % (
                          trace, p.pid, name, ret1, ret2)
                    failures.append(msg)
                    break
        if failures:
            self.fail('\n\n'.join(failures))

    def test_zombies(self):
        # test that NPS is raised by the 2nd implementation in case a
        # process no longer exists
        ZOMBIE_PID = max(psutil.pids()) + 5000
        for name, _ in self.fun_names:
            meth = wrap_exceptions(getattr(_psutil_windows, name))
            self.assertRaises(psutil.NoSuchProcess, meth, ZOMBIE_PID)


def test_main():
    test_suite = unittest.TestSuite()
    test_suite.addTest(unittest.makeSuite(WindowsSpecificTestCase))
    test_suite.addTest(unittest.makeSuite(TestDualProcessImplementation))
    result = unittest.TextTestRunner(verbosity=2).run(test_suite)
    return result.wasSuccessful()

if __name__ == '__main__':
    if not test_main():
        sys.exit(1)
