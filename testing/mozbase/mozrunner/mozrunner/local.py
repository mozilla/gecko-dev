#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

__all__ = ['CLI',
           'cli',
           'package_metadata',
           'LocalRunner',
           'local_runners',
           'FirefoxRunner',
           'MetroFirefoxRunner',
           'ThunderbirdRunner']

import mozinfo
import optparse
import os
import platform
import subprocess
import sys
import ConfigParser

from utils import get_metadata_from_egg
from utils import findInPath
from mozprofile import Profile, FirefoxProfile, MetroFirefoxProfile, ThunderbirdProfile, MozProfileCLI
from runner import Runner

if mozinfo.isMac:
    from plistlib import readPlist

package_metadata = get_metadata_from_egg('mozrunner')

# Map of debugging programs to information about them
# from http://mxr.mozilla.org/mozilla-central/source/build/automationutils.py#59
debuggers = {'gdb': {'interactive': True,
                     'args': ['-q', '--args'],},
             'valgrind': {'interactive': False,
                          'args': ['--leak-check=full']}
             }

def debugger_arguments(debugger, arguments=None, interactive=None):
    """
    finds debugger arguments from debugger given and defaults
    * debugger : debugger name or path to debugger
    * arguments : arguments to the debugger, or None to use defaults
    * interactive : whether the debugger should be run in interactive mode, or None to use default
    """

    # find debugger executable if not a file
    executable = debugger
    if not os.path.exists(executable):
        executable = findInPath(debugger)
    if executable is None:
        raise Exception("Path to '%s' not found" % debugger)

    # if debugger not in dictionary of knowns return defaults
    dirname, debugger = os.path.split(debugger)
    if debugger not in debuggers:
        return ([executable] + (arguments or []), bool(interactive))

    # otherwise use the dictionary values for arguments unless specified
    if arguments is None:
        arguments = debuggers[debugger].get('args', [])
    if interactive is None:
        interactive = debuggers[debugger].get('interactive', False)
    return ([executable] + arguments, interactive)

class LocalRunner(Runner):
    """Handles all running operations. Finds bins, runs and kills the process."""

    profile_class = Profile # profile class to use by default

    @classmethod
    def create(cls, binary=None, cmdargs=None, env=None, kp_kwargs=None, profile_args=None,
               clean_profile=True, process_class=None, **kwargs):
        profile = cls.profile_class(**(profile_args or {}))
        return cls(profile, binary=binary, cmdargs=cmdargs, env=env, kp_kwargs=kp_kwargs,
                                           clean_profile=clean_profile, process_class=process_class, **kwargs)

    def __init__(self, profile, binary, cmdargs=None, env=None,
                 kp_kwargs=None, clean_profile=None, process_class=None, **kwargs):

        super(LocalRunner, self).__init__(profile, clean_profile=clean_profile, kp_kwargs=kp_kwargs,
                                          process_class=process_class, env=env, **kwargs)

        # find the binary
        self.binary = binary
        if not self.binary:
            raise Exception("Binary not specified")
        if not os.path.exists(self.binary):
            raise OSError("Binary path does not exist: %s" % self.binary)

        # To be safe the absolute path of the binary should be used
        self.binary = os.path.abspath(self.binary)

        # allow Mac binaries to be specified as an app bundle
        plist = '%s/Contents/Info.plist' % self.binary
        if mozinfo.isMac and os.path.exists(plist):
            info = readPlist(plist)
            self.binary = os.path.join(self.binary, "Contents/MacOS/",
                                       info['CFBundleExecutable'])

        self.cmdargs = cmdargs or []
        _cmdargs = [i for i in self.cmdargs
                    if i != '-foreground']
        if len(_cmdargs) != len(self.cmdargs):
            # foreground should be last; see
            # https://bugzilla.mozilla.org/show_bug.cgi?id=625614
            self.cmdargs = _cmdargs
            self.cmdargs.append('-foreground')
        if mozinfo.isMac and '-foreground' not in self.cmdargs:
            # runner should specify '-foreground' on Mac; see
            # https://bugzilla.mozilla.org/show_bug.cgi?id=916512
            self.cmdargs.append('-foreground')

        # process environment
        if env is None:
            self.env = os.environ.copy()
        else:
            self.env = env.copy()
        # allows you to run an instance of Firefox separately from any other instances
        self.env['MOZ_NO_REMOTE'] = '1'
        # keeps Firefox attached to the terminal window after it starts
        self.env['NO_EM_RESTART'] = '1'

        # set the library path if needed on linux
        if sys.platform == 'linux2' and self.binary.endswith('-bin'):
            dirname = os.path.dirname(self.binary)
            if os.environ.get('LD_LIBRARY_PATH', None):
                self.env['LD_LIBRARY_PATH'] = '%s:%s' % (os.environ['LD_LIBRARY_PATH'], dirname)
            else:
                self.env['LD_LIBRARY_PATH'] = dirname

    @property
    def command(self):
        """Returns the command list to run."""
        commands = [self.binary, '-profile', self.profile.profile]
        # Bug 775416 - Ensure that binary options are passed in first
        commands[1:1] = self.cmdargs
        return commands

    def get_repositoryInfo(self):
        """Read repository information from application.ini and platform.ini."""

        config = ConfigParser.RawConfigParser()
        dirname = os.path.dirname(self.binary)
        repository = { }

        for file, section in [('application', 'App'), ('platform', 'Build')]:
            config.read(os.path.join(dirname, '%s.ini' % file))

            for key, id in [('SourceRepository', 'repository'),
                            ('SourceStamp', 'changeset')]:
                try:
                    repository['%s_%s' % (file, id)] = config.get(section, key);
                except:
                    repository['%s_%s' % (file, id)] = None

        return repository


    def start(self, debug_args=None, interactive=False, timeout=None, outputTimeout=None):
        """
        Run self.command in the proper environment.
        - debug_args: arguments for the debugger
        - interactive: uses subprocess.Popen directly
        - read_output: sends program output to stdout [default=False]
        - timeout: see process_handler.waitForFinish
        - outputTimeout: see process_handler.waitForFinish
        """

        # ensure you are stopped
        self.stop()

        # ensure the profile exists
        if not self.profile.exists():
            self.profile.reset()
            assert self.profile.exists(), "%s : failure to reset profile" % self.__class__.__name__

        cmd = self._wrap_command(self.command)

        # attach a debugger, if specified
        if debug_args:
            cmd = list(debug_args) + cmd

        if interactive:
            self.process_handler = subprocess.Popen(cmd, env=self.env)
            # TODO: other arguments
        else:
            # this run uses the managed processhandler
            self.process_handler = self.process_class(cmd, env=self.env, **self.kp_kwargs)
            self.process_handler.run(timeout, outputTimeout)


    def _wrap_command(self, cmd):
        """
        If running on OS X 10.5 or older, wrap |cmd| so that it will
        be executed as an i386 binary, in case it's a 32-bit/64-bit universal
        binary.
        """
        if mozinfo.isMac and hasattr(platform, 'mac_ver') and \
                               platform.mac_ver()[0][:4] < '10.6':
            return ["arch", "-arch", "i386"] + cmd
        return cmd


class FirefoxRunner(LocalRunner):
    """Specialized LocalRunner subclass for running Firefox."""

    profile_class = FirefoxProfile

    def __init__(self, profile, binary=None, **kwargs):

        # take the binary from BROWSER_PATH environment variable
        binary = binary or os.environ.get('BROWSER_PATH')
        LocalRunner.__init__(self, profile, binary, **kwargs)


class MetroFirefoxRunner(LocalRunner):
    """Specialized LocalRunner subclass for running Firefox.Metro"""

    profile_class = MetroFirefoxProfile

    # helper application to launch Firefox in Metro mode
    here = os.path.dirname(os.path.abspath(__file__))
    immersiveHelperPath = os.path.sep.join([here,
                                            'resources',
                                            'metrotestharness.exe'])

    def __init__(self, profile, binary=None, **kwargs):

        # take the binary from BROWSER_PATH environment variable
        binary = binary or os.environ.get('BROWSER_PATH')
        LocalRunner.__init__(self, profile, binary, **kwargs)

        if not os.path.exists(self.immersiveHelperPath):
            raise OSError('Can not find Metro launcher: %s' % self.immersiveHelperPath)

        if not mozinfo.isWin:
            raise Exception('Firefox Metro mode is only supported on Windows 8 and onwards')

    @property
    def command(self):
       command = LocalRunner.command.fget(self)
       command[:0] = [self.immersiveHelperPath, '-firefoxpath']

       return command


class ThunderbirdRunner(LocalRunner):
    """Specialized LocalRunner subclass for running Thunderbird"""
    profile_class = ThunderbirdProfile

local_runners = {'firefox': FirefoxRunner,
                 'metrofirefox' : MetroFirefoxRunner,
                 'thunderbird': ThunderbirdRunner}

class CLI(MozProfileCLI):
    """Command line interface."""

    module = "mozrunner"

    def __init__(self, args=sys.argv[1:]):
        """
        Setup command line parser and parse arguments
        - args : command line arguments
        """

        self.metadata = getattr(sys.modules[self.module],
                                'package_metadata',
                                {})
        version = self.metadata.get('Version')
        parser_args = {'description': self.metadata.get('Summary')}
        if version:
            parser_args['version'] = "%prog " + version
        self.parser = optparse.OptionParser(**parser_args)
        self.add_options(self.parser)
        (self.options, self.args) = self.parser.parse_args(args)

        if getattr(self.options, 'info', None):
            self.print_metadata()
            sys.exit(0)

        # choose appropriate runner and profile classes
        try:
            self.runner_class = local_runners[self.options.app]
        except KeyError:
            self.parser.error('Application "%s" unknown (should be one of "%s")' %
                              (self.options.app, ', '.join(local_runners.keys())))

    def add_options(self, parser):
        """add options to the parser"""

        # add profile options
        MozProfileCLI.add_options(self, parser)

        # add runner options
        parser.add_option('-b', "--binary",
                          dest="binary", help="Binary path.",
                          metavar=None, default=None)
        parser.add_option('--app', dest='app', default='firefox',
                          help="Application to use [DEFAULT: %default]")
        parser.add_option('--app-arg', dest='appArgs',
                          default=[], action='append',
                          help="provides an argument to the test application")
        parser.add_option('--debugger', dest='debugger',
                          help="run under a debugger, e.g. gdb or valgrind")
        parser.add_option('--debugger-args', dest='debugger_args',
                          action='store',
                          help="arguments to the debugger")
        parser.add_option('--interactive', dest='interactive',
                          action='store_true',
                          help="run the program interactively")
        if self.metadata:
            parser.add_option("--info", dest="info", default=False,
                              action="store_true",
                              help="Print module information")

    ### methods for introspecting data

    def get_metadata_from_egg(self):
        import pkg_resources
        ret = {}
        dist = pkg_resources.get_distribution(self.module)
        if dist.has_metadata("PKG-INFO"):
            for line in dist.get_metadata_lines("PKG-INFO"):
                key, value = line.split(':', 1)
                ret[key] = value
        if dist.has_metadata("requires.txt"):
            ret["Dependencies"] = "\n" + dist.get_metadata("requires.txt")
        return ret

    def print_metadata(self, data=("Name", "Version", "Summary", "Home-page",
                                   "Author", "Author-email", "License", "Platform", "Dependencies")):
        for key in data:
            if key in self.metadata:
                print key + ": " + self.metadata[key]

    ### methods for running

    def command_args(self):
        """additional arguments for the mozilla application"""
        return self.options.appArgs

    def runner_args(self):
        """arguments to instantiate the runner class"""
        return dict(cmdargs=self.command_args(),
                    binary=self.options.binary,
                    profile_args=self.profile_args())

    def create_runner(self):
        return self.runner_class.create(**self.runner_args())

    def run(self):
        runner = self.create_runner()
        self.start(runner)
        runner.cleanup()

    def debugger_arguments(self):
        """
        returns a 2-tuple of debugger arguments:
        (debugger_arguments, interactive)
        """
        debug_args = self.options.debugger_args
        if debug_args is not None:
            debug_args = debug_args.split()
        interactive = self.options.interactive
        if self.options.debugger:
            debug_args, interactive = debugger_arguments(self.options.debugger, debug_args, interactive)
        return debug_args, interactive

    def start(self, runner):
        """Starts the runner and waits for Firefox to exit or Keyboard Interrupt.
        Shoule be overwritten to provide custom running of the runner instance."""

        # attach a debugger if specified
        debug_args, interactive = self.debugger_arguments()
        runner.start(debug_args=debug_args, interactive=interactive)
        print 'Starting:', ' '.join(runner.command)
        try:
            runner.wait()
        except KeyboardInterrupt:
            runner.stop()


def cli(args=sys.argv[1:]):
    CLI(args).run()

if __name__ == '__main__':
    cli()
