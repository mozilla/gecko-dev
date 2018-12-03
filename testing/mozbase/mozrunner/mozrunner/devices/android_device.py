# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import fileinput
import glob
import os
import platform
import psutil
import re
import shutil
import signal
import sys
import telnetlib
import time
import urlparse
import urllib2
from distutils.spawn import find_executable

from mozdevice import ADBHost, ADBAndroid
from mozprocess import ProcessHandler

EMULATOR_HOME_DIR = os.path.join(os.path.expanduser('~'), '.mozbuild', 'android-device')

EMULATOR_AUTH_FILE = os.path.join(os.path.expanduser('~'), '.emulator_console_auth_token')

TOOLTOOL_URL = 'https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py'

TRY_URL = 'https://hg.mozilla.org/try/raw-file/default'

MANIFEST_PATH = 'testing/config/tooltool-manifests'

verbose_logging = False
devices = {}


class AvdInfo(object):
    """
       Simple class to contain an AVD description.
    """

    def __init__(self, description, name, tooltool_manifest, extra_args,
                 x86):
        self.description = description
        self.name = name
        self.tooltool_manifest = tooltool_manifest
        self.extra_args = extra_args
        self.x86 = x86


"""
   A dictionary to map an AVD type to a description of that type of AVD.

   There is one entry for each type of AVD used in Mozilla automated tests
   and the parameters for each reflect those used in mozharness.
"""
AVD_DICT = {
    '4.3': AvdInfo('Android 4.3',
                   'mozemulator-4.3',
                   'testing/config/tooltool-manifests/androidarm_4_3/mach-emulator.manifest',
                   ['-skip-adb-auth', '-verbose', '-show-kernel'],
                   False),
    '7.0': AvdInfo('Android 7.0',
                   'mozemulator-7.0',
                   'testing/config/tooltool-manifests/androidarm_7_0/mach-emulator.manifest',
                   ['-skip-adb-auth', '-verbose',
                    # does not boot if '-show-kernel' -- how strange!
                    '-ranchu',
                    '-selinux', 'permissive',
                    '-memory', '3072', '-cores', '4'],
                   False),
    'x86': AvdInfo('Android 4.2 x86',
                   'mozemulator-x86',
                   'testing/config/tooltool-manifests/androidx86/mach-emulator.manifest',
                   ['-skip-adb-auth', '-verbose', '-show-kernel',
                    '-qemu', '-m', '1024', '-enable-kvm'],
                   True),
    'x86-7.0': AvdInfo('Android 7.0 x86',
                       'mozemulator-x86-7.0',
                       'testing/config/tooltool-manifests/androidx86_7_0/mach-emulator.manifest',
                       ['-skip-adb-auth', '-verbose', '-show-kernel',
                        '-ranchu',
                        '-engine', 'qemu2',
                        '-selinux', 'permissive',
                        '-memory', '3072', '-cores', '4',
                        '-qemu', '-enable-kvm'],
                       True)
}


def _get_device(substs, device_serial=None):
    global devices
    if device_serial in devices:
        device = devices[device_serial]
    else:
        adb_path = _find_sdk_exe(substs, 'adb', False)
        if not adb_path:
            adb_path = 'adb'
        device = ADBAndroid(adb=adb_path, verbose=verbose_logging, device=device_serial)
        devices[device_serial] = device
    return device


def _install_host_utils(build_obj):
    _log_info("Installing host utilities. This may take a while...")
    installed = False
    host_platform = _get_host_platform()
    if host_platform:
        path = os.path.join(MANIFEST_PATH, host_platform, 'hostutils.manifest')
        _get_tooltool_manifest(build_obj.substs, path, EMULATOR_HOME_DIR,
                               'releng.manifest')
        _tooltool_fetch()
        xre_path = glob.glob(os.path.join(EMULATOR_HOME_DIR, 'host-utils*'))
        for path in xre_path:
            if os.path.isdir(path) and os.path.isfile(os.path.join(path, 'xpcshell')):
                os.environ['MOZ_HOST_BIN'] = path
                installed = True
                break
        if not installed:
            _log_warning("Unable to install host utilities.")
    else:
        _log_warning(
            "Unable to install host utilities -- your platform is not supported!")


def _maybe_update_host_utils(build_obj):
    """
       Compare the installed host-utils to the version name in the manifest;
       if the installed version is older, offer to update.
    """

    # Determine existing/installed version
    existing_path = None
    xre_paths = glob.glob(os.path.join(EMULATOR_HOME_DIR, 'host-utils*'))
    for path in xre_paths:
        if os.path.isdir(path) and os.path.isfile(os.path.join(path, 'xpcshell')):
            existing_path = path
            break
    if existing_path is None:
        # if not installed, no need to upgrade (new version will be installed)
        return
    existing_version = os.path.basename(existing_path)

    # Determine manifest version
    manifest_version = None
    host_platform = _get_host_platform()
    if host_platform:
        # Extract tooltool file name from manifest, something like:
        #     "filename": "host-utils-58.0a1.en-US-linux-x86_64.tar.gz",
        manifest_path = os.path.join(MANIFEST_PATH, host_platform, 'hostutils.manifest')
        with open(manifest_path, 'r') as f:
            for line in f.readlines():
                m = re.search('.*\"(host-utils-.*)\"', line)
                if m:
                    manifest_version = m.group(1)
                    break

    # Compare, prompt, update
    if existing_version and manifest_version:
        manifest_version = manifest_version[:len(existing_version)]
        if existing_version < manifest_version:
            _log_info("Your host utilities are out of date!")
            _log_info("You have %s installed, but %s is available" %
                      (existing_version, manifest_version))
            response = raw_input(
                "Update host utilities? (Y/n) ").strip()
            if response.lower().startswith('y') or response == '':
                parts = os.path.split(existing_path)
                backup_dir = '_backup-' + parts[1]
                backup_path = os.path.join(parts[0], backup_dir)
                shutil.move(existing_path, backup_path)
                _install_host_utils(build_obj)


def verify_android_device(build_obj, install=False, xre=False, debugger=False,
                          verbose=False, app=None, device_serial=None):
    """
       Determine if any Android device is connected via adb.
       If no device is found, prompt to start an emulator.
       If a device is found or an emulator started and 'install' is
       specified, also check whether Firefox is installed on the
       device; if not, prompt to install Firefox.
       If 'xre' is specified, also check with MOZ_HOST_BIN is set
       to a valid xre/host-utils directory; if not, prompt to set
       one up.
       If 'debugger' is specified, also check that JimDB is installed;
       if JimDB is not found, prompt to set up JimDB.
       Returns True if the emulator was started or another device was
       already connected.
    """
    device_verified = False
    emulator = AndroidEmulator('*', substs=build_obj.substs, verbose=verbose)
    adb_path = _find_sdk_exe(build_obj.substs, 'adb', False)
    if not adb_path:
        adb_path = 'adb'
    adbhost = ADBHost(adb=adb_path, verbose=verbose, timeout=10)
    devices = adbhost.devices(timeout=10)
    if 'device' in [d['state'] for d in devices]:
        device_verified = True
    elif emulator.is_available():
        response = raw_input(
            "No Android devices connected. Start an emulator? (Y/n) ").strip()
        if response.lower().startswith('y') or response == '':
            if not emulator.check_avd():
                _log_info("Fetching AVD. This may take a while...")
                emulator.update_avd()
            _log_info("Starting emulator running %s..." %
                      emulator.get_avd_description())
            emulator.start()
            emulator.wait_for_start()
            device_verified = True

    if device_verified and "DEVICE_SERIAL" not in os.environ:
        devices = adbhost.devices(timeout=10)
        for d in devices:
            if d['state'] == 'device':
                os.environ["DEVICE_SERIAL"] = d['device_serial']
                break

    if device_verified and install:
        # Determine if Firefox is installed on the device; if not,
        # prompt to install. This feature allows a test command to
        # launch an emulator, install Firefox, and proceed with testing
        # in one operation. It is also a basic safeguard against other
        # cases where testing is requested but Firefox installation has
        # been forgotten.
        # If Firefox is installed, there is no way to determine whether
        # the current build is installed, and certainly no way to
        # determine if the installed build is the desired build.
        # Installing every time is problematic because:
        #  - it prevents testing against other builds (downloaded apk)
        #  - installation may take a couple of minutes.
        if not app:
            app = build_obj.substs["ANDROID_PACKAGE_NAME"]
        device = _get_device(build_obj.substs, device_serial)
        response = ''
        while not device.is_app_installed(app):
            try:
                if 'fennec' in app or 'firefox' in app:
                    response = response = raw_input(
                        "It looks like %s is not installed on this device.\n"
                        "Install Firefox? (Y/n) or quit to exit " % app).strip()
                    if response.lower().startswith('y') or response == '':
                        _log_info("Installing Firefox. This may take a while...")
                        build_obj._run_make(directory=".", target='install',
                                            ensure_exit_code=False)
                elif app == 'org.mozilla.geckoview.test':
                    response = response = raw_input(
                        "It looks like %s is not installed on this device.\n"
                        "Install geckoview AndroidTest? (Y/n) or quit to exit " % app).strip()
                    if response.lower().startswith('y') or response == '':
                        _log_info("Installing geckoview AndroidTest. This may take a while...")
                        sub = 'geckoview:installWithGeckoBinariesDebugAndroidTest'
                        build_obj._mach_context.commands.dispatch('gradle',
                                                                  args=[sub],
                                                                  context=build_obj._mach_context)
                elif app == 'org.mozilla.geckoview_example':
                    response = response = raw_input(
                        "It looks like %s is not installed on this device.\n"
                        "Install geckoview_example? (Y/n) or quit to exit " % app).strip()
                    if response.lower().startswith('y') or response == '':
                        _log_info("Installing geckoview_example. This may take a while...")
                        sub = 'install-geckoview_example'
                        build_obj._mach_context.commands.dispatch('android',
                                                                  subcommand=sub,
                                                                  args=[],
                                                                  context=build_obj._mach_context)
                else:
                    response = raw_input(
                        "It looks like %s is not installed on this device,\n"
                        "but I don't know how to install it.\n"
                        "Install it now, then hit Enter or quit to exit " % app)
            except EOFError:
                response = 'quit'
            if response == 'quit':
                device_verified = False
                break

    if device_verified and xre:
        # Check whether MOZ_HOST_BIN has been set to a valid xre; if not,
        # prompt to install one.
        xre_path = os.environ.get('MOZ_HOST_BIN')
        err = None
        if not xre_path:
            err = "environment variable MOZ_HOST_BIN is not set to a directory " \
                  "containing host xpcshell"
        elif not os.path.isdir(xre_path):
            err = '$MOZ_HOST_BIN does not specify a directory'
        elif not os.path.isfile(os.path.join(xre_path, 'xpcshell')):
            err = '$MOZ_HOST_BIN/xpcshell does not exist'
        if err:
            _maybe_update_host_utils(build_obj)
            xre_path = glob.glob(os.path.join(EMULATOR_HOME_DIR, 'host-utils*'))
            for path in xre_path:
                if os.path.isdir(path) and os.path.isfile(os.path.join(path, 'xpcshell')):
                    os.environ['MOZ_HOST_BIN'] = path
                    err = None
                    break
        if err:
            _log_info("Host utilities not found: %s" % err)
            response = raw_input(
                "Download and setup your host utilities? (Y/n) ").strip()
            if response.lower().startswith('y') or response == '':
                _install_host_utils(build_obj)

    if debugger:
        # Optionally set up JimDB. See https://wiki.mozilla.org/Mobile/Fennec/Android/GDB.
        build_platform = _get_device_platform(build_obj.substs)
        jimdb_path = os.path.join(EMULATOR_HOME_DIR, 'jimdb-%s' % build_platform)
        jimdb_utils_path = os.path.join(jimdb_path, 'utils')
        gdb_path = os.path.join(jimdb_path, 'bin', 'gdb')
        err = None
        if not os.path.isdir(jimdb_path):
            err = '%s does not exist' % jimdb_path
        elif not os.path.isfile(gdb_path):
            err = '%s not found' % gdb_path
        if err:
            _log_info("JimDB (%s) not found: %s" % (build_platform, err))
            response = raw_input(
                "Download and setup JimDB (%s)? (Y/n) " % build_platform).strip()
            if response.lower().startswith('y') or response == '':
                host_platform = _get_host_platform()
                if host_platform:
                    _log_info(
                        "Installing JimDB (%s/%s). This may take a while..." % (host_platform,
                                                                                build_platform))
                    path = os.path.join(MANIFEST_PATH, host_platform,
                                        'jimdb-%s.manifest' % build_platform)
                    _get_tooltool_manifest(build_obj.substs, path,
                                           EMULATOR_HOME_DIR, 'releng.manifest')
                    _tooltool_fetch()
                    if os.path.isfile(gdb_path):
                        # Get JimDB utilities from git repository
                        proc = ProcessHandler(['git', 'pull'], cwd=jimdb_utils_path)
                        proc.run()
                        git_pull_complete = False
                        try:
                            proc.wait()
                            if proc.proc.returncode == 0:
                                git_pull_complete = True
                        except Exception:
                            if proc.poll() is None:
                                proc.kill(signal.SIGTERM)
                        if not git_pull_complete:
                            _log_warning("Unable to update JimDB utils from git -- "
                                         "some JimDB features may be unavailable.")
                    else:
                        _log_warning("Unable to install JimDB -- unable to fetch from tooltool.")
                else:
                    _log_warning("Unable to install JimDB -- your platform is not supported!")
        if os.path.isfile(gdb_path):
            # sync gdbinit.local with build settings
            _update_gdbinit(build_obj.substs, os.path.join(jimdb_utils_path, "gdbinit.local"))
            # ensure JimDB is in system path, so that mozdebug can find it
            bin_path = os.path.join(jimdb_path, 'bin')
            os.environ['PATH'] = "%s:%s" % (bin_path, os.environ['PATH'])

    return device_verified


def get_adb_path(build_obj):
    return _find_sdk_exe(build_obj.substs, 'adb', False)


def run_firefox_for_android(build_obj, params):
    """
       Launch Firefox for Android on the connected device.
       Optional 'params' allow parameters to be passed to Firefox.
    """
    device = _get_device(build_obj.substs)
    try:
        #
        # Construct an adb command similar to:
        #
        # $ adb shell am start -a android.activity.MAIN \
        #   -n org.mozilla.fennec_$USER \
        #   -d <url param> \
        #   --es args "<params>"
        #
        app = build_obj.substs['ANDROID_PACKAGE_NAME']
        url = None
        if params:
            for p in params:
                if urlparse.urlparse(p).scheme != "":
                    url = p
                    params.remove(p)
                    break
        device.launch_fennec(app, extra_args=params, url=url)
    except Exception:
        _log_warning("unable to launch Firefox for Android")
        return 1
    return 0


def grant_runtime_permissions(build_obj, app, device_serial=None):
    """
    Grant required runtime permissions to the specified app
    (typically org.mozilla.fennec_$USER).
    """
    device = _get_device(build_obj.substs, device_serial)
    try:
        sdk_level = device.version
        if sdk_level and sdk_level >= 23:
            _log_info("Granting important runtime permissions to %s" % app)
            device.shell_output('pm grant %s android.permission.WRITE_EXTERNAL_STORAGE' % app)
            device.shell_output('pm grant %s android.permission.READ_EXTERNAL_STORAGE' % app)
            device.shell_output('pm grant %s android.permission.ACCESS_COARSE_LOCATION' % app)
            device.shell_output('pm grant %s android.permission.ACCESS_FINE_LOCATION' % app)
            device.shell_output('pm grant %s android.permission.CAMERA' % app)
            device.shell_output('pm grant %s android.permission.RECORD_AUDIO' % app)
    except Exception:
        _log_warning("Unable to grant runtime permissions to %s" % app)


class AndroidEmulator(object):

    """
        Support running the Android emulator with an AVD from Mozilla
        test automation.

        Example usage:
            emulator = AndroidEmulator()
            if not emulator.is_running() and emulator.is_available():
                if not emulator.check_avd():
                    warn("this may take a while...")
                    emulator.update_avd()
                emulator.start()
                emulator.wait_for_start()
                emulator.wait()
    """

    def __init__(self, avd_type=None, verbose=False, substs=None, device_serial=None):
        global verbose_logging
        self.emulator_log = None
        self.emulator_path = 'emulator'
        verbose_logging = verbose
        self.substs = substs
        self.avd_type = self._get_avd_type(avd_type)
        self.avd_info = AVD_DICT[self.avd_type]
        self.gpu = True
        self.restarted = False
        self.device_serial = device_serial
        _log_debug("Running on %s" % platform.platform())
        _log_debug("Emulator created with type %s" % self.avd_type)

    def __del__(self):
        if self.emulator_log:
            self.emulator_log.close()

    def is_running(self):
        """
           Returns True if the Android emulator is running.
        """
        for proc in psutil.process_iter():
            name = proc.name()
            # On some platforms, "emulator" may start an emulator with
            # process name "emulator64-arm" or similar.
            if name and name.startswith('emulator'):
                return True
        return False

    def is_available(self):
        """
           Returns True if an emulator executable is found.
        """
        found = False
        emulator_path = _find_sdk_exe(self.substs, 'emulator', True)
        if emulator_path:
            self.emulator_path = emulator_path
            found = True
        return found

    def check_avd(self, force=False):
        """
           Determine if the AVD is already installed locally.
           (This is usually used to determine if update_avd() is likely
           to require a download; it is a convenient way of determining
           whether a 'this may take a while' warning is warranted.)

           Returns True if the AVD is installed.
        """
        avd = os.path.join(
            EMULATOR_HOME_DIR, 'avd', self.avd_info.name + '.avd')
        if force and os.path.exists(avd):
            shutil.rmtree(avd)
        if os.path.exists(avd):
            _log_debug("AVD found at %s" % avd)
            return True
        return False

    def update_avd(self, force=False):
        """
           If required, update the AVD via tooltool.

           If the AVD directory is not found, or "force" is requested,
           download the tooltool manifest associated with the AVD and then
           invoke tooltool.py on the manifest. tooltool.py will download the
           required archive (unless already present in the local tooltool
           cache) and install the AVD.
        """
        avd = os.path.join(
            EMULATOR_HOME_DIR, 'avd', self.avd_info.name + '.avd')
        ini_file = os.path.join(
            EMULATOR_HOME_DIR, 'avd', self.avd_info.name + '.ini')
        if force and os.path.exists(avd):
            shutil.rmtree(avd)
        if not os.path.exists(avd):
            if os.path.exists(ini_file):
                os.remove(ini_file)
            path = self.avd_info.tooltool_manifest
            _get_tooltool_manifest(self.substs, path, EMULATOR_HOME_DIR, 'releng.manifest')
            _tooltool_fetch()
            self._update_avd_paths()

    def start(self):
        """
           Launch the emulator.
        """
        if self.avd_info.x86 and 'linux' in _get_host_platform():
            _verify_kvm(self.substs)
        if os.path.exists(EMULATOR_AUTH_FILE):
            os.remove(EMULATOR_AUTH_FILE)
            _log_debug("deleted %s" % EMULATOR_AUTH_FILE)
        # create an empty auth file to disable emulator authentication
        auth_file = open(EMULATOR_AUTH_FILE, 'w')
        auth_file.close()

        def outputHandler(line):
            self.emulator_log.write("<%s>\n" % line)
            if "Invalid value for -gpu" in line or "Invalid GPU mode" in line:
                self.gpu = False
        env = os.environ
        env['ANDROID_AVD_HOME'] = os.path.join(EMULATOR_HOME_DIR, "avd")
        command = [self.emulator_path, "-avd", self.avd_info.name]
        if self.gpu:
            command += ['-gpu', 'swiftshader_indirect']
        if self.avd_info.extra_args:
            # -enable-kvm option is not valid on OSX
            if _get_host_platform() == 'macosx64' and '-enable-kvm' in self.avd_info.extra_args:
                self.avd_info.extra_args.remove('-enable-kvm')
            command += self.avd_info.extra_args
        log_path = os.path.join(EMULATOR_HOME_DIR, 'emulator.log')
        self.emulator_log = open(log_path, 'w')
        _log_debug("Starting the emulator with this command: %s" %
                   ' '.join(command))
        _log_debug("Emulator output will be written to '%s'" %
                   log_path)
        self.proc = ProcessHandler(
            command, storeOutput=False, processOutputLine=outputHandler,
            env=env, ignore_children=True)
        self.proc.run()
        _log_debug("Emulator started with pid %d" %
                   int(self.proc.proc.pid))

    def wait_for_start(self):
        """
           Verify that the emulator is running, the emulator device is visible
           to adb, and Android has booted.
        """
        if not self.proc:
            _log_warning("Emulator not started!")
            return False
        if self.check_completed():
            return False
        _log_debug("Waiting for device status...")
        adb_path = _find_sdk_exe(self.substs, 'adb', False)
        if not adb_path:
            adb_path = 'adb'
        adbhost = ADBHost(adb=adb_path, verbose=verbose_logging, timeout=10)
        devs = adbhost.devices(timeout=10)
        devs = [(d['device_serial'], d['state']) for d in devs]
        while ('emulator-5554', 'device') not in devs:
            time.sleep(10)
            if self.check_completed():
                return False
            devs = adbhost.devices(timeout=10)
            devs = [(d['device_serial'], d['state']) for d in devs]
        _log_debug("Device status verified.")

        _log_debug("Checking that Android has booted...")
        device = _get_device(self.substs, self.device_serial)
        complete = False
        while not complete:
            output = ''
            try:
                output = device.get_prop('sys.boot_completed', timeout=5)
            except Exception:
                # adb not yet responding...keep trying
                pass
            if output.strip() == '1':
                complete = True
            else:
                time.sleep(10)
                if self.check_completed():
                    return False
        _log_debug("Android boot status verified.")

        if not self._verify_emulator():
            return False
        if self.avd_info.x86:
            _log_info("Running the x86 emulator; be sure to install an x86 APK!")
        else:
            _log_info("Running the arm emulator; be sure to install an arm APK!")
        return True

    def check_completed(self):
        if self.proc.proc.poll() is not None:
            if not self.gpu and not self.restarted:
                _log_warning("Emulator failed to start. Your emulator may be out of date.")
                _log_warning("Trying to restart the emulator without -gpu argument.")
                self.restarted = True
                self.start()
                return False
            _log_warning("Emulator has already completed!")
            log_path = os.path.join(EMULATOR_HOME_DIR, 'emulator.log')
            _log_warning("See log at %s and/or use --verbose for more information." % log_path)
            return True
        return False

    def wait(self):
        """
           Wait for the emulator to close. If interrupted, close the emulator.
        """
        try:
            self.proc.wait()
        except Exception:
            if self.proc.poll() is None:
                self.cleanup()
        return self.proc.poll()

    def cleanup(self):
        """
           Close the emulator.
        """
        self.proc.kill(signal.SIGTERM)

    def get_avd_description(self):
        """
           Return the human-friendly description of this AVD.
        """
        return self.avd_info.description

    def _update_avd_paths(self):
        avd_path = os.path.join(EMULATOR_HOME_DIR, "avd")
        ini_file = os.path.join(avd_path, "test-1.ini")
        ini_file_new = os.path.join(avd_path, self.avd_info.name + ".ini")
        os.rename(ini_file, ini_file_new)
        avd_dir = os.path.join(avd_path, "test-1.avd")
        avd_dir_new = os.path.join(avd_path, self.avd_info.name + ".avd")
        os.rename(avd_dir, avd_dir_new)
        self._replace_ini_contents(ini_file_new)

    def _replace_ini_contents(self, path):
        with open(path, "r") as f:
            lines = f.readlines()
        with open(path, "w") as f:
            for line in lines:
                if line.startswith('path='):
                    avd_path = os.path.join(EMULATOR_HOME_DIR, "avd")
                    f.write('path=%s/%s.avd\n' %
                            (avd_path, self.avd_info.name))
                elif line.startswith('path.rel='):
                    f.write('path.rel=avd/%s.avd\n' % self.avd_info.name)
                else:
                    f.write(line)

    def _telnet_cmd(self, telnet, command):
        _log_debug(">>> " + command)
        telnet.write('%s\n' % command)
        result = telnet.read_until('OK', 10)
        _log_debug("<<< " + result)
        return result

    def _verify_emulator(self):
        telnet_ok = False
        tn = None
        while(not telnet_ok):
            try:
                tn = telnetlib.Telnet('localhost', 5554, 10)
                if tn is not None:
                    tn.read_until('OK', 10)
                    self._telnet_cmd(tn, 'avd status')
                    self._telnet_cmd(tn, 'redir list')
                    self._telnet_cmd(tn, 'network status')
                    tn.write('quit\n')
                    tn.read_all()
                    telnet_ok = True
                else:
                    _log_warning("Unable to connect to port 5554")
            except Exception:
                _log_warning("Trying again after unexpected exception")
            finally:
                if tn is not None:
                    tn.close()
            if not telnet_ok:
                time.sleep(10)
                if self.proc.proc.poll() is not None:
                    _log_warning("Emulator has already completed!")
                    return False
        return telnet_ok

    def _get_avd_type(self, requested):
        if requested in AVD_DICT.keys():
            return requested
        if self.substs:
            if not self.substs['TARGET_CPU'].startswith('arm'):
                return 'x86-7.0'
            else:
                return '7.0'
        return 'x86-7.0'


def _find_sdk_exe(substs, exe, tools):
    if tools:
        subdirs = ['emulator', 'tools']
    else:
        subdirs = ['platform-tools']

    found = False
    if not found and substs:
        # It's best to use the tool specified by the build, rather
        # than something we find on the PATH or crawl for.
        try:
            exe_path = substs[exe.upper()]
            if os.path.exists(exe_path):
                found = True
            else:
                _log_debug(
                    "Unable to find executable at %s" % exe_path)
        except KeyError:
            _log_debug("%s not set" % exe.upper())

    # Append '.exe' to the name on Windows if it's not present,
    # so that the executable can be found.
    if (os.name == 'nt' and not exe.lower().endswith('.exe')):
        exe += '.exe'

    if not found:
        # Can exe be found in the Android SDK?
        try:
            android_sdk_root = os.environ['ANDROID_SDK_ROOT']
            for subdir in subdirs:
                exe_path = os.path.join(
                    android_sdk_root, subdir, exe)
                if os.path.exists(exe_path):
                    found = True
                    break
                else:
                    _log_debug(
                        "Unable to find executable at %s" % exe_path)
        except KeyError:
            _log_debug("ANDROID_SDK_ROOT not set")

    if not found:
        # Can exe be found in the default bootstrap location?
        mozbuild_path = os.environ.get('MOZBUILD_STATE_PATH',
                                       os.path.expanduser(os.path.join('~', '.mozbuild')))
        for subdir in subdirs:
            exe_path = os.path.join(
                mozbuild_path, 'android-sdk-linux', subdir, exe)
            if os.path.exists(exe_path):
                found = True
                break
            else:
                _log_debug(
                    "Unable to find executable at %s" % exe_path)

    if not found:
        # Is exe on PATH?
        exe_path = find_executable(exe)
        if exe_path:
            found = True
        else:
            _log_debug("Unable to find executable on PATH")

    if found:
        _log_debug("%s found at %s" % (exe, exe_path))
        try:
            creation_time = os.path.getctime(exe_path)
            _log_debug("  ...with creation time %s" % time.ctime(creation_time))
        except Exception:
            _log_warning("Could not get creation time for %s" % exe_path)

        prop_path = os.path.join(os.path.dirname(exe_path), "source.properties")
        if os.path.exists(prop_path):
            with open(prop_path, 'r') as f:
                for line in f.readlines():
                    if line.startswith("Pkg.Revision"):
                        line = line.strip()
                        _log_debug("  ...with SDK version in %s: %s" % (prop_path, line))
                        break
    else:
        exe_path = None
    return exe_path


def _log_debug(text):
    if verbose_logging:
        print("DEBUG: %s" % text)


def _log_warning(text):
    print("WARNING: %s" % text)


def _log_info(text):
    print("%s" % text)


def _download_file(url, filename, path):
    _log_debug("Download %s to %s/%s..." % (url, path, filename))
    f = urllib2.urlopen(url)
    if not os.path.isdir(path):
        try:
            os.makedirs(path)
        except Exception as e:
            _log_warning(str(e))
            return False
    local_file = open(os.path.join(path, filename), 'wb')
    local_file.write(f.read())
    local_file.close()
    _log_debug("Downloaded %s to %s/%s" % (url, path, filename))
    return True


def _get_tooltool_manifest(substs, src_path, dst_path, filename):
    if not os.path.isdir(dst_path):
        try:
            os.makedirs(dst_path)
        except Exception as e:
            _log_warning(str(e))
    copied = False
    if substs and 'top_srcdir' in substs:
        src = os.path.join(substs['top_srcdir'], src_path)
        if os.path.exists(src):
            dst = os.path.join(dst_path, filename)
            shutil.copy(src, dst)
            copied = True
            _log_debug("Copied tooltool manifest %s to %s" % (src, dst))
    if not copied:
        url = os.path.join(TRY_URL, src_path)
        _download_file(url, filename, dst_path)


def _tooltool_fetch():
    def outputHandler(line):
        _log_debug(line)
    _download_file(TOOLTOOL_URL, 'tooltool.py', EMULATOR_HOME_DIR)
    command = [sys.executable, 'tooltool.py',
               'fetch', '-o', '-m', 'releng.manifest']
    proc = ProcessHandler(
        command, processOutputLine=outputHandler, storeOutput=False,
        cwd=EMULATOR_HOME_DIR)
    proc.run()
    try:
        proc.wait()
    except Exception:
        if proc.poll() is None:
            proc.kill(signal.SIGTERM)


def _get_host_platform():
    plat = None
    if 'darwin' in str(sys.platform).lower():
        plat = 'macosx64'
    elif 'linux' in str(sys.platform).lower():
        if '64' in platform.architecture()[0]:
            plat = 'linux64'
        else:
            plat = 'linux32'
    return plat


def _get_device_platform(substs):
    # PIE executables are required when SDK level >= 21 - important for gdbserver
    device = _get_device(substs)
    sdk_level = device.version
    pie = ''
    if sdk_level and sdk_level >= 21:
        pie = '-pie'
    if substs['TARGET_CPU'].startswith('arm'):
        return 'arm%s' % pie
    return 'x86%s' % pie


def _update_gdbinit(substs, path):
    if os.path.exists(path):
        obj_replaced = False
        src_replaced = False
        # update existing objdir/srcroot in place
        for line in fileinput.input(path, inplace=True):
            if "feninit.default.objdir" in line and substs and 'MOZ_BUILD_ROOT' in substs:
                print("python feninit.default.objdir = '%s'" % substs['MOZ_BUILD_ROOT'])
                obj_replaced = True
            elif "feninit.default.srcroot" in line and substs and 'top_srcdir' in substs:
                print("python feninit.default.srcroot = '%s'" % substs['top_srcdir'])
                src_replaced = True
            else:
                print(line.strip())
        # append objdir/srcroot if not updated
        if (not obj_replaced) and substs and 'MOZ_BUILD_ROOT' in substs:
            with open(path, "a") as f:
                f.write("\npython feninit.default.objdir = '%s'\n" % substs['MOZ_BUILD_ROOT'])
        if (not src_replaced) and substs and 'top_srcdir' in substs:
            with open(path, "a") as f:
                f.write("python feninit.default.srcroot = '%s'\n" % substs['top_srcdir'])
    else:
        # write objdir/srcroot to new gdbinit file
        with open(path, "w") as f:
            if substs and 'MOZ_BUILD_ROOT' in substs:
                f.write("python feninit.default.objdir = '%s'\n" % substs['MOZ_BUILD_ROOT'])
            if substs and 'top_srcdir' in substs:
                f.write("python feninit.default.srcroot = '%s'\n" % substs['top_srcdir'])


def _verify_kvm(substs):
    # 'emulator -accel-check' should produce output like:
    # accel:
    # 0
    # KVM (version 12) is installed and usable
    # accel
    emulator_path = _find_sdk_exe(substs, 'emulator', True)
    if not emulator_path:
        emulator_path = 'emulator'
    command = [emulator_path, '-accel-check']
    try:
        p = ProcessHandler(command, storeOutput=True)
        p.run()
        p.wait()
        out = p.output
        if 'is installed and usable' in ''.join(out):
            return
    except Exception as e:
        _log_warning(str(e))
    _log_warning("Unable to verify kvm acceleration!")
    _log_warning("The x86 emulator may fail to start without kvm.")
