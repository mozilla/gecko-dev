# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import mozdevice
import os
import re


def get_dm(marionette=None, **kwargs):
    dm_type = os.environ.get('DM_TRANS', 'adb')
    if marionette and hasattr(marionette.runner, 'device'):
        return marionette.runner.app_ctx.dm
    else:
        if dm_type == 'adb':
            return mozdevice.DeviceManagerADB(deviceSerial=marionette.device_serial,
                                              serverHost=marionette.adb_host,
                                              serverPort=marionette.adb_port,
                                              **kwargs)
        elif dm_type == 'sut':
            host = os.environ.get('TEST_DEVICE')
            if not host:
                raise Exception('Must specify host with SUT!')
            return mozdevice.DeviceManagerSUT(host=host)
        else:
            raise Exception('Unknown device manager type: %s' % dm_type)


def get_b2g_pid(dm):
    b2g_output = dm.shellCheckOutput(['b2g-ps']).split('\n')
    first_line = b2g_output[0].split()
    app_index = first_line.index('APPLICATION')
    pid_index = first_line.index('PID')
    for line in b2g_output:
        split_line = line.split()
        if split_line[app_index] == 'b2g':
            return split_line[pid_index]


class B2GTestCaseMixin(object):

    # TODO: add methods like 'restart b2g'
    def __init__(self, *args, **kwargs):
        self._device_manager = None

    def get_device_manager(self, *args, **kwargs):
        capabilities = self.marionette.session and \
            self.marionette.session_capabilities or {}
        if not self._device_manager and \
                capabilities.get('device') != 'desktop':
            self._device_manager = get_dm(self.marionette, **kwargs)
        return self._device_manager

    @property
    def device_manager(self):
        return self.get_device_manager()


class B2GTestResultMixin(object):

    def __init__(self, *args, **kwargs):
        self.result_modifiers.append(self.b2g_output_modifier)
        self.b2g_pid = kwargs.pop('b2g_pid')
        self.logcat_stdout = kwargs.pop('logcat_stdout')

    def _diagnose_socket(self):
        # This function will check if b2g is running and report any recent errors. This is
        # used in automation since a plain timeout error doesn't tell you
        # much information about what actually is going on

        extra_output = None
        dm_type = os.environ.get('DM_TRANS', 'adb')
        if dm_type == 'adb':
            device_manager = get_dm(self.marionette)
            pid = get_b2g_pid(device_manager)
            if pid:
                # find recent errors
                message = ""
                error_re = re.compile(r"""[\s\S]*(exception|error)[\s\S]*""",
                                      flags=re.IGNORECASE)
                logcat = device_manager.getLogcat()
                # Due to Bug 1050211
                if len(logcat) == 1:
                    logcat = logcat[0].splitlines()
                latest = []
                iters = len(logcat) - 1
                # reading from the latest line
                while len(latest) < 5 and iters >= 0:
                    line = logcat[iters]
                    error_log_line = error_re.match(line)
                    if error_log_line is not None:
                        latest.append(line)
                    iters -= 1
                message += "\nMost recent errors/exceptions are:\n"
                for line in reversed(latest):
                    message += "%s" % line
                b2g_status = ""
                if pid != self.b2g_pid:
                    b2g_status = "The B2G process has restarted after crashing during  the tests so "
                else:
                    b2g_status = "B2G is still running but "
                extra_output = ("%s\n%sMarionette can't respond due to either a Gecko, Gaia or Marionette error. "
                                "Above, the 5 most recent errors are listed. "
                                "Check logcat for all errors if these errors are not the cause "
                                "of the failure." % (message, b2g_status))
            else:
                extra_output = "B2G process has died"
        return extra_output

    def b2g_output_modifier(self, test, result_expected, result_actual, output, context):
        # output is the actual string output from the test, so we have to do string comparison
        if "IOError" in output or "Broken pipe" in output or "Connection timed out" in output:
            extra_output = self._diagnose_socket()
            if extra_output:
                self.logger.error(extra_output)
                output += extra_output

        if self.logcat_stdout:
            dm = get_dm(self.marionette)
            for next_line in dm.getLogcat():
                self.logger.info(next_line)
            self.logger.info("--------- end logcat")
            dm.shellCheckOutput(['/system/bin/logcat', '-c'])

        return result_expected, result_actual, output, context
