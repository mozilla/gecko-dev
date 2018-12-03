# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import hashlib
import itertools
import json
import logging
import operator
import os
import re
import subprocess
import sys
import tempfile
import yaml

from collections import OrderedDict

import mozpack.path as mozpath

from mach.decorators import (
    CommandArgument,
    CommandArgumentGroup,
    CommandProvider,
    Command,
    SettingsProvider,
    SubCommand,
)

from mach.main import Mach

from mozbuild.base import (
    BuildEnvironmentNotFoundException,
    MachCommandBase,
    MachCommandConditions as conditions,
    MozbuildObject,
)
from mozbuild.util import ensureParentDir

from mozbuild.backend import (
    backends,
)


BUILD_WHAT_HELP = '''
What to build. Can be a top-level make target or a relative directory. If
multiple options are provided, they will be built serially. Takes dependency
information from `topsrcdir/build/dumbmake-dependencies` to build additional
targets as needed. BUILDING ONLY PARTS OF THE TREE CAN RESULT IN BAD TREE
STATE. USE AT YOUR OWN RISK.
'''.strip()


EXCESSIVE_SWAP_MESSAGE = '''
===================
PERFORMANCE WARNING

Your machine experienced a lot of swap activity during the build. This is
possibly a sign that your machine doesn't have enough physical memory or
not enough available memory to perform the build. It's also possible some
other system activity during the build is to blame.

If you feel this message is not appropriate for your machine configuration,
please file a Firefox Build System :: General bug at
https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox%20Build%20System&component=General
and tell us about your machine and build configuration so we can adjust the
warning heuristic.
===================
'''


class StoreDebugParamsAndWarnAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        sys.stderr.write('The --debugparams argument is deprecated. Please ' +
                         'use --debugger-args instead.\n\n')
        setattr(namespace, self.dest, values)


@CommandProvider
class Watch(MachCommandBase):
    """Interface to watch and re-build the tree."""

    @Command('watch', category='post-build', description='Watch and re-build the tree.',
             conditions=[conditions.is_firefox])
    @CommandArgument('-v', '--verbose', action='store_true',
                     help='Verbose output for what commands the watcher is running.')
    def watch(self, verbose=False):
        """Watch and re-build the source tree."""

        if not conditions.is_artifact_build(self):
            print('mach watch requires an artifact build. See '
                  'https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Simple_Firefox_build')
            return 1

        if not self.substs.get('WATCHMAN', None):
            print('mach watch requires watchman to be installed. See '
                  'https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Incremental_builds_with_filesystem_watching')
            return 1

        self._activate_virtualenv()
        try:
            self.virtualenv_manager.install_pip_package('pywatchman==1.3.0')
        except Exception:
            print('Could not install pywatchman from pip. See '
                  'https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Incremental_builds_with_filesystem_watching')
            return 1

        from mozbuild.faster_daemon import Daemon
        daemon = Daemon(self.config_environment)

        try:
            return daemon.watch()
        except KeyboardInterrupt:
            # Suppress ugly stack trace when user hits Ctrl-C.
            sys.exit(3)


@CommandProvider
class Build(MachCommandBase):
    """Interface to build the tree."""

    @Command('build', category='build', description='Build the tree.')
    @CommandArgument('--jobs', '-j', default='0', metavar='jobs', type=int,
        help='Number of concurrent jobs to run. Default is the number of CPUs.')
    @CommandArgument('-C', '--directory', default=None,
        help='Change to a subdirectory of the build directory first.')
    @CommandArgument('what', default=None, nargs='*', help=BUILD_WHAT_HELP)
    @CommandArgument('-X', '--disable-extra-make-dependencies',
                     default=False, action='store_true',
                     help='Do not add extra make dependencies.')
    @CommandArgument('-v', '--verbose', action='store_true',
        help='Verbose output for what commands the build is running.')
    @CommandArgument('--keep-going', action='store_true',
                     help='Keep building after an error has occurred')
    def build(self, what=None, disable_extra_make_dependencies=None, jobs=0,
        directory=None, verbose=False, keep_going=False):
        """Build the source tree.

        With no arguments, this will perform a full build.

        Positional arguments define targets to build. These can be make targets
        or patterns like "<dir>/<target>" to indicate a make target within a
        directory.

        There are a few special targets that can be used to perform a partial
        build faster than what `mach build` would perform:

        * binaries - compiles and links all C/C++ sources and produces shared
          libraries and executables (binaries).

        * faster - builds JavaScript, XUL, CSS, etc files.

        "binaries" and "faster" almost fully complement each other. However,
        there are build actions not captured by either. If things don't appear to
        be rebuilding, perform a vanilla `mach build` to rebuild the world.
        """
        from mozbuild.controller.building import (
            BuildDriver,
        )

        self.log_manager.enable_all_structured_loggers()

        driver = self._spawn(BuildDriver)
        return driver.build(
            what=what,
            disable_extra_make_dependencies=disable_extra_make_dependencies,
            jobs=jobs,
            directory=directory,
            verbose=verbose,
            keep_going=keep_going,
            mach_context=self._mach_context)

    @Command('configure', category='build',
        description='Configure the tree (run configure and config.status).')
    @CommandArgument('options', default=None, nargs=argparse.REMAINDER,
                     help='Configure options')
    def configure(self, options=None, buildstatus_messages=False, line_handler=None):
        from mozbuild.controller.building import (
            BuildDriver,
        )

        self.log_manager.enable_all_structured_loggers()
        driver = self._spawn(BuildDriver)

        return driver.configure(
            options=options,
            buildstatus_messages=buildstatus_messages,
            line_handler=line_handler)

    @Command('resource-usage', category='post-build',
        description='Show information about system resource usage for a build.')
    @CommandArgument('--address', default='localhost',
        help='Address the HTTP server should listen on.')
    @CommandArgument('--port', type=int, default=0,
        help='Port number the HTTP server should listen on.')
    @CommandArgument('--browser', default='firefox',
        help='Web browser to automatically open. See webbrowser Python module.')
    @CommandArgument('--url',
        help='URL of JSON document to display')
    def resource_usage(self, address=None, port=None, browser=None, url=None):
        import webbrowser
        from mozbuild.html_build_viewer import BuildViewerServer

        server = BuildViewerServer(address, port)

        if url:
            server.add_resource_json_url('url', url)
        else:
            last = self._get_state_filename('build_resources.json')
            if not os.path.exists(last):
                print('Build resources not available. If you have performed a '
                    'build and receive this message, the psutil Python package '
                    'likely failed to initialize properly.')
                return 1

            server.add_resource_json_file('last', last)
        try:
            webbrowser.get(browser).open_new_tab(server.url)
        except Exception:
            print('Cannot get browser specified, trying the default instead.')
            try:
                browser = webbrowser.get().open_new_tab(server.url)
            except Exception:
                print('Please open %s in a browser.' % server.url)

        print('Hit CTRL+c to stop server.')
        server.run()

    @Command('build-backend', category='build',
        description='Generate a backend used to build the tree.')
    @CommandArgument('-d', '--diff', action='store_true',
        help='Show a diff of changes.')
    # It would be nice to filter the choices below based on
    # conditions, but that is for another day.
    @CommandArgument('-b', '--backend', nargs='+', choices=sorted(backends),
        help='Which backend to build.')
    @CommandArgument('-v', '--verbose', action='store_true',
        help='Verbose output.')
    @CommandArgument('-n', '--dry-run', action='store_true',
        help='Do everything except writing files out.')
    def build_backend(self, backend, diff=False, verbose=False, dry_run=False):
        python = self.virtualenv_manager.python_path
        config_status = os.path.join(self.topobjdir, 'config.status')

        if not os.path.exists(config_status):
            print('config.status not found.  Please run |mach configure| '
                  'or |mach build| prior to building the %s build backend.'
                  % backend)
            return 1

        args = [python, config_status]
        if backend:
            args.append('--backend')
            args.extend(backend)
        if diff:
            args.append('--diff')
        if verbose:
            args.append('--verbose')
        if dry_run:
            args.append('--dry-run')

        return self._run_command_in_objdir(args=args, pass_thru=True,
            ensure_exit_code=False)


@CommandProvider
class CargoProvider(MachCommandBase):
    """Invoke cargo in useful ways."""

    @Command('cargo', category='build',
             description='Invoke cargo in useful ways.')
    def cargo(self):
        self.parser.print_usage()
        return 1

    @SubCommand('cargo', 'check',
                description='Run `cargo check` on a given crate.  Defaults to gkrust.')
    @CommandArgument('--all-crates', default=None, action='store_true',
        help='Check all of the crates in the tree.')
    @CommandArgument('crates', default=None, nargs='*', help='The crate name(s) to check.')
    def check(self, all_crates=None, crates=None):
        # XXX duplication with `mach vendor rust`
        crates_and_roots = {
            'gkrust': 'toolkit/library/rust',
            'gkrust-gtest': 'toolkit/library/gtest/rust',
            'js': 'js/rust',
            'mozjs_sys': 'js/src',
            'baldrdash': 'js/src/wasm/cranelift',
            'geckodriver': 'testing/geckodriver',
        }

        if all_crates:
            crates = crates_and_roots.keys()
        elif crates == None or crates == []:
            crates = ['gkrust']

        for crate in crates:
            root = crates_and_roots.get(crate, None)
            if not root:
                print('Cannot locate crate %s.  Please check your spelling or '
                      'add the crate information to the list.' % crate)
                return 1

            check_targets = [
                'force-cargo-library-check',
                'force-cargo-host-library-check',
                'force-cargo-program-check',
                'force-cargo-host-program-check',
            ]

            ret = self._run_make(srcdir=False, directory=root,
                                 ensure_exit_code=0, silent=True,
                                 print_directory=False, target=check_targets)
            if ret != 0:
                return ret

        return 0

@CommandProvider
class Doctor(MachCommandBase):
    """Provide commands for diagnosing common build environment problems"""
    @Command('doctor', category='devenv',
        description='')
    @CommandArgument('--fix', default=None, action='store_true',
        help='Attempt to fix found problems.')
    def doctor(self, fix=None):
        self._activate_virtualenv()
        from mozbuild.doctor import Doctor
        doctor = Doctor(self.topsrcdir, self.topobjdir, fix)
        return doctor.check_all()

@CommandProvider
class Clobber(MachCommandBase):
    NO_AUTO_LOG = True
    CLOBBER_CHOICES = ['objdir', 'python']
    @Command('clobber', category='build',
        description='Clobber the tree (delete the object directory).')
    @CommandArgument('what', default=['objdir'], nargs='*',
        help='Target to clobber, must be one of {{{}}} (default objdir).'.format(
             ', '.join(CLOBBER_CHOICES)))
    @CommandArgument('--full', action='store_true',
        help='Perform a full clobber')
    def clobber(self, what, full=False):
        """Clean up the source and object directories.

        Performing builds and running various commands generate various files.

        Sometimes it is necessary to clean up these files in order to make
        things work again. This command can be used to perform that cleanup.

        By default, this command removes most files in the current object
        directory (where build output is stored). Some files (like Visual
        Studio project files) are not removed by default. If you would like
        to remove the object directory in its entirety, run with `--full`.

        The `python` target will clean up various generated Python files from
        the source directory and will remove untracked files from well-known
        directories containing Python packages. Run this to remove .pyc files,
        compiled C extensions, etc. Note: all files not tracked or ignored by
        version control in well-known Python package directories will be
        deleted. Run the `status` command of your VCS to see if any untracked
        files you haven't committed yet will be deleted.
        """
        invalid = set(what) - set(self.CLOBBER_CHOICES)
        if invalid:
            print('Unknown clobber target(s): {}'.format(', '.join(invalid)))
            return 1

        ret = 0
        if 'objdir' in what:
            from mozbuild.controller.clobber import Clobberer
            try:
                Clobberer(self.topsrcdir, self.topobjdir).remove_objdir(full)
            except OSError as e:
                if sys.platform.startswith('win'):
                    if isinstance(e, WindowsError) and e.winerror in (5,32):
                        self.log(logging.ERROR, 'file_access_error', {'error': e},
                            "Could not clobber because a file was in use. If the "
                            "application is running, try closing it. {error}")
                        return 1
                raise

        if 'python' in what:
            if conditions.is_hg(self):
                cmd = ['hg', 'purge', '--all', '-I', 'glob:**.py[cdo]',
                       '-I', 'path:python/', '-I', 'path:third_party/python/']
            elif conditions.is_git(self):
                cmd = ['git', 'clean', '-f', '-x', '*.py[cdo]', 'python/',
                       'third_party/python/']
            else:
                # We don't know what is tracked/untracked if we don't have VCS.
                # So we can't clean python/ and third_party/python/.
                cmd = ['find', '.', '-type', 'f', '-name', '*.py[cdo]',
                       '-delete']
            ret = subprocess.call(cmd, cwd=self.topsrcdir)
        return ret

    @property
    def substs(self):
        try:
            return super(Clobber, self).substs
        except BuildEnvironmentNotFoundException:
            return {}

@CommandProvider
class Logs(MachCommandBase):
    """Provide commands to read mach logs."""
    NO_AUTO_LOG = True

    @Command('show-log', category='post-build',
        description='Display mach logs')
    @CommandArgument('log_file', nargs='?', type=argparse.FileType('rb'),
        help='Filename to read log data from. Defaults to the log of the last '
             'mach command.')
    def show_log(self, log_file=None):
        if not log_file:
            path = self._get_state_filename('last_log.json')
            log_file = open(path, 'rb')

        if os.isatty(sys.stdout.fileno()):
            env = dict(os.environ)
            if 'LESS' not in env:
                # Sensible default flags if none have been set in the user
                # environment.
                env[b'LESS'] = b'FRX'
            less = subprocess.Popen(['less'], stdin=subprocess.PIPE, env=env)
            # Various objects already have a reference to sys.stdout, so we
            # can't just change it, we need to change the file descriptor under
            # it to redirect to less's input.
            # First keep a copy of the sys.stdout file descriptor.
            output_fd = os.dup(sys.stdout.fileno())
            os.dup2(less.stdin.fileno(), sys.stdout.fileno())

        startTime = 0
        for line in log_file:
            created, action, params = json.loads(line)
            if not startTime:
                startTime = created
                self.log_manager.terminal_handler.formatter.start_time = \
                    created
            if 'line' in params:
                record = logging.makeLogRecord({
                    'created': created,
                    'name': self._logger.name,
                    'levelno': logging.INFO,
                    'msg': '{line}',
                    'params': params,
                    'action': action,
                })
                self._logger.handle(record)

        if self.log_manager.terminal:
            # Close less's input so that it knows that we're done sending data.
            less.stdin.close()
            # Since the less's input file descriptor is now also the stdout
            # file descriptor, we still actually have a non-closed system file
            # descriptor for less's input. Replacing sys.stdout's file
            # descriptor with what it was before we replaced it will properly
            # close less's input.
            os.dup2(output_fd, sys.stdout.fileno())
            less.wait()


@CommandProvider
class Warnings(MachCommandBase):
    """Provide commands for inspecting warnings."""

    @property
    def database_path(self):
        return self._get_state_filename('warnings.json')

    @property
    def database(self):
        from mozbuild.compilation.warnings import WarningsDatabase

        path = self.database_path

        database = WarningsDatabase()

        if os.path.exists(path):
            database.load_from_file(path)

        return database

    @Command('warnings-summary', category='post-build',
        description='Show a summary of compiler warnings.')
    @CommandArgument('-C', '--directory', default=None,
        help='Change to a subdirectory of the build directory first.')
    @CommandArgument('report', default=None, nargs='?',
        help='Warnings report to display. If not defined, show the most '
            'recent report.')
    def summary(self, directory=None, report=None):
        database = self.database

        if directory:
            dirpath = self.join_ensure_dir(self.topsrcdir, directory)
            if not dirpath:
                return 1
        else:
            dirpath = None

        type_counts = database.type_counts(dirpath)
        sorted_counts = sorted(type_counts.iteritems(),
            key=operator.itemgetter(1))

        total = 0
        for k, v in sorted_counts:
            print('%d\t%s' % (v, k))
            total += v

        print('%d\tTotal' % total)

    @Command('warnings-list', category='post-build',
        description='Show a list of compiler warnings.')
    @CommandArgument('-C', '--directory', default=None,
        help='Change to a subdirectory of the build directory first.')
    @CommandArgument('--flags', default=None, nargs='+',
        help='Which warnings flags to match.')
    @CommandArgument('report', default=None, nargs='?',
        help='Warnings report to display. If not defined, show the most '
            'recent report.')
    def list(self, directory=None, flags=None, report=None):
        database = self.database

        by_name = sorted(database.warnings)

        topsrcdir = mozpath.normpath(self.topsrcdir)

        if directory:
            directory = mozpath.normsep(directory)
            dirpath = self.join_ensure_dir(topsrcdir, directory)
            if not dirpath:
                return 1

        if flags:
            # Flatten lists of flags.
            flags = set(itertools.chain(*[flaglist.split(',') for flaglist in flags]))

        for warning in by_name:
            filename = mozpath.normsep(warning['filename'])

            if filename.startswith(topsrcdir):
                filename = filename[len(topsrcdir) + 1:]

            if directory and not filename.startswith(directory):
                continue

            if flags and warning['flag'] not in flags:
                continue

            if warning['column'] is not None:
                print('%s:%d:%d [%s] %s' % (filename, warning['line'],
                    warning['column'], warning['flag'], warning['message']))
            else:
                print('%s:%d [%s] %s' % (filename, warning['line'],
                    warning['flag'], warning['message']))

    def join_ensure_dir(self, dir1, dir2):
        dir1 = mozpath.normpath(dir1)
        dir2 = mozpath.normsep(dir2)
        joined_path = mozpath.join(dir1, dir2)
        if os.path.isdir(joined_path):
            return joined_path
        else:
            print('Specified directory not found.')
            return None

@CommandProvider
class GTestCommands(MachCommandBase):
    @Command('gtest', category='testing',
        description='Run GTest unit tests (C++ tests).')
    @CommandArgument('gtest_filter', default=b"*", nargs='?', metavar='gtest_filter',
        help="test_filter is a ':'-separated list of wildcard patterns (called the positive patterns),"
             "optionally followed by a '-' and another ':'-separated pattern list (called the negative patterns).")
    @CommandArgument('--jobs', '-j', default='1', nargs='?', metavar='jobs', type=int,
        help='Run the tests in parallel using multiple processes.')
    @CommandArgument('--tbpl-parser', '-t', action='store_true',
        help='Output test results in a format that can be parsed by TBPL.')
    @CommandArgument('--shuffle', '-s', action='store_true',
        help='Randomize the execution order of tests.')

    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
        help='Enable the debugger. Not specifying a --debugger option will result in the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
        help='Name of debugger to use.')
    @CommandArgument('--debugger-args', default=None, metavar='params', type=str,
        group='debugging',
        help='Command-line arguments to pass to the debugger itself; split as the Bourne shell would.')

    def gtest(self, shuffle, jobs, gtest_filter, tbpl_parser, debug, debugger,
              debugger_args):

        # We lazy build gtest because it's slow to link
        try:
            config = self.config_environment
        except Exception:
            print("Please run |./mach build| before |./mach gtest|.")
            return 1

        active_backend = config.substs.get('BUILD_BACKENDS', [None])[0]
        if 'Tup' in active_backend:
            gtest_build_target = mozpath.join(self.topobjdir, '<gtest>')
        else:
            gtest_build_target = 'recurse_gtest'

        res = self._mach_context.commands.dispatch('build', self._mach_context,
                                                   what=[gtest_build_target])
        if res:
            print("Could not build xul-gtest")
            return res

        if self.substs.get('MOZ_WIDGET_TOOLKIT') == 'cocoa':
            self._run_make(directory='browser/app', target='repackage',
                           ensure_exit_code=True)

        app_path = self.get_binary_path('app')
        args = [app_path, '-unittest', '--gtest_death_test_style=threadsafe'];

        if sys.platform.startswith('win') and \
            'MOZ_LAUNCHER_PROCESS' in self.defines:
            args.append('--wait-for-browser')

        if debug or debugger or debugger_args:
            args = self.prepend_debugger_args(args, debugger, debugger_args)

        cwd = os.path.join(self.topobjdir, '_tests', 'gtest')

        if not os.path.isdir(cwd):
            os.makedirs(cwd)

        # Use GTest environment variable to control test execution
        # For details see:
        # https://code.google.com/p/googletest/wiki/AdvancedGuide#Running_Test_Programs:_Advanced_Options
        gtest_env = {b'GTEST_FILTER': gtest_filter}

        # Note: we must normalize the path here so that gtest on Windows sees
        # a MOZ_GMP_PATH which has only Windows dir seperators, because
        # nsIFile cannot open the paths with non-Windows dir seperators.
        xre_path = os.path.join(os.path.normpath(self.topobjdir), "dist", "bin")
        gtest_env["MOZ_XRE_DIR"] = xre_path
        gtest_env["MOZ_GMP_PATH"] = os.pathsep.join(
            os.path.join(xre_path, p, "1.0")
            for p in ('gmp-fake', 'gmp-fakeopenh264')
        )

        gtest_env[b"MOZ_RUN_GTEST"] = b"True"

        if shuffle:
            gtest_env[b"GTEST_SHUFFLE"] = b"True"

        if tbpl_parser:
            gtest_env[b"MOZ_TBPL_PARSER"] = b"True"

        if jobs == 1:
            return self.run_process(args=args,
                                    append_env=gtest_env,
                                    cwd=cwd,
                                    ensure_exit_code=False,
                                    pass_thru=True)

        from mozprocess import ProcessHandlerMixin
        import functools
        def handle_line(job_id, line):
            # Prepend the jobId
            line = '[%d] %s' % (job_id + 1, line.strip())
            self.log(logging.INFO, "GTest", {'line': line}, '{line}')

        gtest_env["GTEST_TOTAL_SHARDS"] = str(jobs)
        processes = {}
        for i in range(0, jobs):
            gtest_env["GTEST_SHARD_INDEX"] = str(i)
            processes[i] = ProcessHandlerMixin([app_path, "-unittest"],
                             cwd=cwd,
                             env=gtest_env,
                             processOutputLine=[functools.partial(handle_line, i)],
                             universal_newlines=True)
            processes[i].run()

        exit_code = 0
        for process in processes.values():
            status = process.wait()
            if status:
                exit_code = status

        # Clamp error code to 255 to prevent overflowing multiple of
        # 256 into 0
        if exit_code > 255:
            exit_code = 255

        return exit_code

    def prepend_debugger_args(self, args, debugger, debugger_args):
        '''
        Given an array with program arguments, prepend arguments to run it under a
        debugger.

        :param args: The executable and arguments used to run the process normally.
        :param debugger: The debugger to use, or empty to use the default debugger.
        :param debugger_args: Any additional parameters to pass to the debugger.
        '''

        import mozdebug

        if not debugger:
            # No debugger name was provided. Look for the default ones on
            # current OS.
            debugger = mozdebug.get_default_debugger_name(mozdebug.DebuggerSearch.KeepLooking)

        if debugger:
            debuggerInfo = mozdebug.get_debugger_info(debugger, debugger_args)
            if not debuggerInfo:
                print("Could not find a suitable debugger in your PATH.")
                return 1

        # Parameters come from the CLI. We need to convert them before
        # their use.
        if debugger_args:
            from mozbuild import shellutil
            try:
                debugger_args = shellutil.split(debugger_args)
            except shellutil.MetaCharacterException as e:
                print("The --debugger_args you passed require a real shell to parse them.")
                print("(We can't handle the %r character.)" % e.char)
                return 1

        # Prepend the debugger args.
        args = [debuggerInfo.path] + debuggerInfo.args + args
        return args

@CommandProvider
class ClangCommands(MachCommandBase):
    @Command('clang-complete', category='devenv',
        description='Generate a .clang_complete file.')
    def clang_complete(self):
        import shlex

        build_vars = {}

        def on_line(line):
            elements = [s.strip() for s in line.split('=', 1)]

            if len(elements) != 2:
                return

            build_vars[elements[0]] = elements[1]

        try:
            old_logger = self.log_manager.replace_terminal_handler(None)
            self._run_make(target='showbuild', log=False, line_handler=on_line)
        finally:
            self.log_manager.replace_terminal_handler(old_logger)

        def print_from_variable(name):
            if name not in build_vars:
                return

            value = build_vars[name]

            value = value.replace('-I.', '-I%s' % self.topobjdir)
            value = value.replace(' .', ' %s' % self.topobjdir)
            value = value.replace('-I..', '-I%s/..' % self.topobjdir)
            value = value.replace(' ..', ' %s/..' % self.topobjdir)

            args = shlex.split(value)
            for i in range(0, len(args) - 1):
                arg = args[i]

                if arg.startswith(('-I', '-D')):
                    print(arg)
                    continue

                if arg.startswith('-include'):
                    print(arg + ' ' + args[i + 1])
                    continue

        print_from_variable('COMPILE_CXXFLAGS')

        print('-I%s/ipc/chromium/src' % self.topsrcdir)
        print('-I%s/ipc/glue' % self.topsrcdir)
        print('-I%s/ipc/ipdl/_ipdlheaders' % self.topobjdir)


@CommandProvider
class Package(MachCommandBase):
    """Package the built product for distribution."""

    @Command('package', category='post-build',
        description='Package the built product for distribution as an APK, DMG, etc.')
    @CommandArgument('-v', '--verbose', action='store_true',
        help='Verbose output for what commands the packaging process is running.')
    def package(self, verbose=False):
        ret = self._run_make(directory=".", target='package',
                             silent=not verbose, ensure_exit_code=False)
        if ret == 0:
            self.notify('Packaging complete')
        return ret

@CommandProvider
class Install(MachCommandBase):
    """Install a package."""

    @Command('install', category='post-build',
        description='Install the package on the machine, or on a device.')
    @CommandArgument('--verbose', '-v', action='store_true',
        help='Print verbose output when installing to an Android emulator.')
    def install(self, verbose=False):
        if conditions.is_android(self):
            from mozrunner.devices.android_device import verify_android_device
            verify_android_device(self, verbose=verbose)
        ret = self._run_make(directory=".", target='install', ensure_exit_code=False)
        if ret == 0:
            self.notify('Install complete')
        return ret

@SettingsProvider
class RunSettings():
    config_settings = [
        ('runprefs.*', 'string', """
Pass a pref into Firefox when using `mach run`, of the form `foo.bar=value`.
Prefs will automatically be cast into the appropriate type. Integers can be
single quoted to force them to be strings.
""".strip()),
    ]

@CommandProvider
class RunProgram(MachCommandBase):
    """Run the compiled program."""

    prog_group = 'the compiled program'

    @Command('run', category='post-build',
        description='Run the compiled program, possibly under a debugger or DMD.')
    @CommandArgument('params', nargs='...', group=prog_group,
        help='Command-line arguments to be passed through to the program. Not specifying a --profile or -P option will result in a temporary profile being used.')
    @CommandArgumentGroup(prog_group)
    @CommandArgument('--remote', '-r', action='store_true', group=prog_group,
        help='Do not pass the --no-remote argument by default.')
    @CommandArgument('--background', '-b', action='store_true', group=prog_group,
        help='Do not pass the --foreground argument by default on Mac.')
    @CommandArgument('--noprofile', '-n', action='store_true', group=prog_group,
        help='Do not pass the --profile argument by default.')
    @CommandArgument('--disable-e10s', action='store_true', group=prog_group,
        help='Run the program with electrolysis disabled.')
    @CommandArgument('--enable-crash-reporter', action='store_true', group=prog_group,
        help='Run the program with the crash reporter enabled.')
    @CommandArgument('--setpref', action='append', default=[], group=prog_group,
        help='Set the specified pref before starting the program. Can be set multiple times. Prefs can also be set in ~/.mozbuild/machrc in the [runprefs] section - see `./mach settings` for more information.')
    @CommandArgument('--temp-profile', action='store_true', group=prog_group,
        help='Run the program using a new temporary profile created inside the objdir.')
    @CommandArgument('--macos-open', action='store_true', group=prog_group,
        help="On macOS, run the program using the open(1) command. Per open(1), the browser is launched \"just as if you had double-clicked the file's icon\". The browser can not be launched under a debugger with this option.")

    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
        help='Enable the debugger. Not specifying a --debugger option will result in the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
        help='Name of debugger to use.')
    @CommandArgument('--debugger-args', default=None, metavar='params', type=str,
        group='debugging',
        help='Command-line arguments to pass to the debugger itself; split as the Bourne shell would.')
    @CommandArgument('--debugparams', action=StoreDebugParamsAndWarnAction,
        default=None, type=str, dest='debugger_args', group='debugging',
        help=argparse.SUPPRESS)

    @CommandArgumentGroup('DMD')
    @CommandArgument('--dmd', action='store_true', group='DMD',
        help='Enable DMD. The following arguments have no effect without this.')
    @CommandArgument('--mode', choices=['live', 'dark-matter', 'cumulative', 'scan'], group='DMD',
         help='Profiling mode. The default is \'dark-matter\'.')
    @CommandArgument('--stacks', choices=['partial', 'full'], group='DMD',
        help='Allocation stack trace coverage. The default is \'partial\'.')
    @CommandArgument('--show-dump-stats', action='store_true', group='DMD',
        help='Show stats when doing dumps.')
    def run(self, params, remote, background, noprofile, disable_e10s,
        enable_crash_reporter, setpref, temp_profile, macos_open, debug,
        debugger, debugger_args, dmd, mode, stacks, show_dump_stats):

        if conditions.is_android(self):
            # Running Firefox for Android is completely different
            if dmd:
                print("DMD is not supported for Firefox for Android")
                return 1
            from mozrunner.devices.android_device import verify_android_device, run_firefox_for_android
            if not (debug or debugger or debugger_args):
                verify_android_device(self, install=True)
                return run_firefox_for_android(self, params)
            verify_android_device(self, install=True, debugger=True)
            args = ['']

        else:
            from mozprofile import Profile, Preferences

            try:
                binpath = self.get_binary_path('app')
            except Exception as e:
                print("It looks like your program isn't built.",
                    "You can run |mach build| to build it.")
                print(e)
                return 1

            args = []
            if macos_open:
                if debug:
                    print("The browser can not be launched in the debugger "
                        "when using the macOS open command.")
                    return 1
                try:
                    m = re.search(r'^.+\.app', binpath)
                    apppath = m.group(0)
                    args = ['open', apppath, '--args']
                except Exception as e:
                    print("Couldn't get the .app path from the binary path. "
                        "The macOS open option can only be used on macOS")
                    print(e)
                    return 1
            else:
                args = [binpath]

            if params:
                args.extend(params)

            if not remote:
                args.append('-no-remote')

            if not background and sys.platform == 'darwin':
                args.append('-foreground')

            if sys.platform.startswith('win') and \
               'MOZ_LAUNCHER_PROCESS' in self.defines:
                args.append('-wait-for-browser')

            no_profile_option_given = \
                all(p not in params for p in ['-profile', '--profile', '-P'])
            if no_profile_option_given and not noprofile:
                prefs = {
                   'browser.shell.checkDefaultBrowser': False,
                   'general.warnOnAboutConfig': False,
                }
                prefs.update(self._mach_context.settings.runprefs)
                prefs.update([p.split('=', 1) for p in setpref])
                for pref in prefs:
                    prefs[pref] = Preferences.cast(prefs[pref])

                tmpdir = os.path.join(self.topobjdir, 'tmp')
                if not os.path.exists(tmpdir):
                    os.makedirs(tmpdir)

                if (temp_profile):
                    path = tempfile.mkdtemp(dir=tmpdir, prefix='profile-')
                else:
                    path = os.path.join(tmpdir, 'profile-default')

                profile = Profile(path, preferences=prefs)
                args.append('-profile')
                args.append(profile.profile)

            if not no_profile_option_given and setpref:
                print("setpref is only supported if a profile is not specified")
                return 1

        extra_env = {
            'MOZ_DEVELOPER_REPO_DIR': self.topsrcdir,
            'MOZ_DEVELOPER_OBJ_DIR': self.topobjdir,
            'RUST_BACKTRACE': 'full',
        }

        if not enable_crash_reporter:
            extra_env['MOZ_CRASHREPORTER_DISABLE'] = '1'
        else:
            extra_env['MOZ_CRASHREPORTER'] = '1'

        if disable_e10s:
            extra_env['MOZ_FORCE_DISABLE_E10S'] = '1'

        if debug or debugger or debugger_args:
            if 'INSIDE_EMACS' in os.environ:
                self.log_manager.terminal_handler.setLevel(logging.WARNING)

            import mozdebug
            if not debugger:
                # No debugger name was provided. Look for the default ones on
                # current OS.
                debugger = mozdebug.get_default_debugger_name(mozdebug.DebuggerSearch.KeepLooking)

            if debugger:
                self.debuggerInfo = mozdebug.get_debugger_info(debugger, debugger_args)
                if not self.debuggerInfo:
                    print("Could not find a suitable debugger in your PATH.")
                    return 1

            # Parameters come from the CLI. We need to convert them before
            # their use.
            if debugger_args:
                from mozbuild import shellutil
                try:
                    debugger_args = shellutil.split(debugger_args)
                except shellutil.MetaCharacterException as e:
                    print("The --debugger-args you passed require a real shell to parse them.")
                    print("(We can't handle the %r character.)" % e.char)
                    return 1

            # Prepend the debugger args.
            args = [self.debuggerInfo.path] + self.debuggerInfo.args + args

        if dmd:
            dmd_params = []

            if mode:
                dmd_params.append('--mode=' + mode)
            if stacks:
                dmd_params.append('--stacks=' + stacks)
            if show_dump_stats:
                dmd_params.append('--show-dump-stats=yes')

            if dmd_params:
                extra_env['DMD'] = ' '.join(dmd_params)
            else:
                extra_env['DMD'] = '1'

        return self.run_process(args=args, ensure_exit_code=False,
            pass_thru=True, append_env=extra_env)

@CommandProvider
class Buildsymbols(MachCommandBase):
    """Produce a package of debug symbols suitable for use with Breakpad."""

    @Command('buildsymbols', category='post-build',
        description='Produce a package of Breakpad-format symbols.')
    def buildsymbols(self):
        return self._run_make(directory=".", target='buildsymbols', ensure_exit_code=False)

@CommandProvider
class Makefiles(MachCommandBase):
    @Command('empty-makefiles', category='build-dev',
        description='Find empty Makefile.in in the tree.')
    def empty(self):
        import pymake.parser
        import pymake.parserdata

        IGNORE_VARIABLES = {
            'DEPTH': ('@DEPTH@',),
            'topsrcdir': ('@top_srcdir@',),
            'srcdir': ('@srcdir@',),
            'relativesrcdir': ('@relativesrcdir@',),
            'VPATH': ('@srcdir@',),
        }

        IGNORE_INCLUDES = [
            'include $(DEPTH)/config/autoconf.mk',
            'include $(topsrcdir)/config/config.mk',
            'include $(topsrcdir)/config/rules.mk',
        ]

        def is_statement_relevant(s):
            if isinstance(s, pymake.parserdata.SetVariable):
                exp = s.vnameexp
                if not exp.is_static_string:
                    return True

                if exp.s not in IGNORE_VARIABLES:
                    return True

                return s.value not in IGNORE_VARIABLES[exp.s]

            if isinstance(s, pymake.parserdata.Include):
                if s.to_source() in IGNORE_INCLUDES:
                    return False

            return True

        for path in self._makefile_ins():
            relpath = os.path.relpath(path, self.topsrcdir)
            try:
                statements = [s for s in pymake.parser.parsefile(path)
                    if is_statement_relevant(s)]

                if not statements:
                    print(relpath)
            except pymake.parser.SyntaxError:
                print('Warning: Could not parse %s' % relpath, file=sys.stderr)

    def _makefile_ins(self):
        for root, dirs, files in os.walk(self.topsrcdir):
            for f in files:
                if f == 'Makefile.in':
                    yield os.path.join(root, f)

@CommandProvider
class MachDebug(MachCommandBase):
    @Command('environment', category='build-dev',
        description='Show info about the mach and build environment.')
    @CommandArgument('--format', default='pretty',
        choices=['pretty', 'configure', 'json'],
        help='Print data in the given format.')
    @CommandArgument('--output', '-o', type=str,
        help='Output to the given file.')
    @CommandArgument('--verbose', '-v', action='store_true',
        help='Print verbose output.')
    def environment(self, format, output=None, verbose=False):
        func = getattr(self, '_environment_%s' % format.replace('.', '_'))

        if output:
            # We want to preserve mtimes if the output file already exists
            # and the content hasn't changed.
            from mozbuild.util import FileAvoidWrite
            with FileAvoidWrite(output) as out:
                return func(out, verbose)
        return func(sys.stdout, verbose)

    def _environment_pretty(self, out, verbose):
        state_dir = self._mach_context.state_dir
        import platform
        print('platform:\n\t%s' % platform.platform(), file=out)
        print('python version:\n\t%s' % sys.version, file=out)
        print('python prefix:\n\t%s' % sys.prefix, file=out)
        print('mach cwd:\n\t%s' % self._mach_context.cwd, file=out)
        print('os cwd:\n\t%s' % os.getcwd(), file=out)
        print('mach directory:\n\t%s' % self._mach_context.topdir, file=out)
        print('state directory:\n\t%s' % state_dir, file=out)

        print('object directory:\n\t%s' % self.topobjdir, file=out)

        if self.mozconfig['path']:
            print('mozconfig path:\n\t%s' % self.mozconfig['path'], file=out)
            if self.mozconfig['configure_args']:
                print('mozconfig configure args:', file=out)
                for arg in self.mozconfig['configure_args']:
                    print('\t%s' % arg, file=out)

            if self.mozconfig['make_extra']:
                print('mozconfig extra make args:', file=out)
                for arg in self.mozconfig['make_extra']:
                    print('\t%s' % arg, file=out)

            if self.mozconfig['make_flags']:
                print('mozconfig make flags:', file=out)
                for arg in self.mozconfig['make_flags']:
                    print('\t%s' % arg, file=out)

        config = None

        try:
            config = self.config_environment

        except Exception:
            pass

        if config:
            print('config topsrcdir:\n\t%s' % config.topsrcdir, file=out)
            print('config topobjdir:\n\t%s' % config.topobjdir, file=out)

            if verbose:
                print('config substitutions:', file=out)
                for k in sorted(config.substs):
                    print('\t%s: %s' % (k, config.substs[k]), file=out)

                print('config defines:', file=out)
                for k in sorted(config.defines):
                    print('\t%s' % k, file=out)

    def _environment_json(self, out, verbose):
        import json
        class EnvironmentEncoder(json.JSONEncoder):
            def default(self, obj):
                if isinstance(obj, MozbuildObject):
                    result = {
                        'topsrcdir': obj.topsrcdir,
                        'topobjdir': obj.topobjdir,
                        'mozconfig': obj.mozconfig,
                    }
                    if verbose:
                        result['substs'] = obj.substs
                        result['defines'] = obj.defines
                    return result
                elif isinstance(obj, set):
                    return list(obj)
                return json.JSONEncoder.default(self, obj)
        json.dump(self, cls=EnvironmentEncoder, sort_keys=True, fp=out)

class ArtifactSubCommand(SubCommand):
    def __call__(self, func):
        after = SubCommand.__call__(self, func)
        jobchoices = {
            'android-api-16',
            'android-x86',
            'linux',
            'linux64',
            'macosx64',
            'win32',
            'win64'
        }
        args = [
            CommandArgument('--tree', metavar='TREE', type=str,
                help='Firefox tree.'),
            CommandArgument('--job', metavar='JOB', choices=jobchoices,
                help='Build job.'),
            CommandArgument('--verbose', '-v', action='store_true',
                help='Print verbose output.'),
        ]
        for arg in args:
            after = arg(after)
        return after


@CommandProvider
class PackageFrontend(MachCommandBase):
    """Fetch and install binary artifacts from Mozilla automation."""

    @Command('artifact', category='post-build',
        description='Use pre-built artifacts to build Firefox.')
    def artifact(self):
        '''Download, cache, and install pre-built binary artifacts to build Firefox.

        Use |mach build| as normal to freshen your installed binary libraries:
        artifact builds automatically download, cache, and install binary
        artifacts from Mozilla automation, replacing whatever may be in your
        object directory.  Use |mach artifact last| to see what binary artifacts
        were last used.

        Never build libxul again!

        '''
        pass

    def _make_artifacts(self, tree=None, job=None, skip_cache=False):
        state_dir = self._mach_context.state_dir
        cache_dir = os.path.join(state_dir, 'package-frontend')

        hg = None
        if conditions.is_hg(self):
            hg = self.substs['HG']

        git = None
        if conditions.is_git(self):
            git = self.substs['GIT']

        from mozbuild.artifacts import Artifacts
        artifacts = Artifacts(tree, self.substs, self.defines, job,
                              log=self.log, cache_dir=cache_dir,
                              skip_cache=skip_cache, hg=hg, git=git,
                              topsrcdir=self.topsrcdir)
        return artifacts

    @ArtifactSubCommand('artifact', 'install',
        'Install a good pre-built artifact.')
    @CommandArgument('source', metavar='SRC', nargs='?', type=str,
        help='Where to fetch and install artifacts from.  Can be omitted, in '
            'which case the current hg repository is inspected; an hg revision; '
            'a remote URL; or a local file.',
        default=None)
    @CommandArgument('--skip-cache', action='store_true',
        help='Skip all local caches to force re-fetching remote artifacts.',
        default=False)
    def artifact_install(self, source=None, skip_cache=False, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        artifacts = self._make_artifacts(tree=tree, job=job, skip_cache=skip_cache)

        return artifacts.install_from(source, self.distdir)

    @ArtifactSubCommand('artifact', 'clear-cache',
        'Delete local artifacts and reset local artifact cache.')
    def artifact_clear_cache(self, tree=None, job=None, verbose=False):
        self._set_log_level(verbose)
        artifacts = self._make_artifacts(tree=tree, job=job)
        artifacts.clear_cache()
        return 0

    @SubCommand('artifact', 'toolchain')
    @CommandArgument('--verbose', '-v', action='store_true',
        help='Print verbose output.')
    @CommandArgument('--cache-dir', metavar='DIR',
        help='Directory where to store the artifacts cache')
    @CommandArgument('--skip-cache', action='store_true',
        help='Skip all local caches to force re-fetching remote artifacts.',
        default=False)
    @CommandArgument('--from-build', metavar='BUILD', nargs='+',
        help='Download toolchains resulting from the given build(s); '
             'BUILD is a name of a toolchain task, e.g. linux64-clang')
    @CommandArgument('--tooltool-manifest', metavar='MANIFEST',
        help='Explicit tooltool manifest to process')
    @CommandArgument('--authentication-file', metavar='FILE',
        help='Use the RelengAPI token found in the given file to authenticate')
    @CommandArgument('--tooltool-url', metavar='URL',
        help='Use the given url as tooltool server')
    @CommandArgument('--no-unpack', action='store_true',
        help='Do not unpack any downloaded file')
    @CommandArgument('--retry', type=int, default=4,
        help='Number of times to retry failed downloads')
    @CommandArgument('--artifact-manifest', metavar='FILE',
        help='Store a manifest about the downloaded taskcluster artifacts')
    @CommandArgument('files', nargs='*',
        help='A list of files to download, in the form path@task-id, in '
             'addition to the files listed in the tooltool manifest.')
    def artifact_toolchain(self, verbose=False, cache_dir=None,
                          skip_cache=False, from_build=(),
                          tooltool_manifest=None, authentication_file=None,
                          tooltool_url=None, no_unpack=False, retry=None,
                          artifact_manifest=None, files=()):
        '''Download, cache and install pre-built toolchains.
        '''
        from mozbuild.artifacts import ArtifactCache
        from mozbuild.action.tooltool import (
            FileRecord,
            open_manifest,
            unpack_file,
        )
        from requests.adapters import HTTPAdapter
        import redo
        import requests
        import shutil

        from taskgraph.util.taskcluster import (
            get_artifact_url,
        )

        self._set_log_level(verbose)
        # Normally, we'd use self.log_manager.enable_unstructured(),
        # but that enables all logging, while we only really want tooltool's
        # and it also makes structured log output twice.
        # So we manually do what it does, and limit that to the tooltool
        # logger.
        if self.log_manager.terminal_handler:
            logging.getLogger('mozbuild.action.tooltool').addHandler(
                self.log_manager.terminal_handler)
            logging.getLogger('redo').addHandler(
                self.log_manager.terminal_handler)
            self.log_manager.terminal_handler.addFilter(
                self.log_manager.structured_filter)
        if not cache_dir:
            cache_dir = os.path.join(self._mach_context.state_dir, 'toolchains')

        tooltool_url = (tooltool_url or
                        'https://tooltool.mozilla-releng.net').rstrip('/')

        cache = ArtifactCache(cache_dir=cache_dir, log=self.log,
                              skip_cache=skip_cache)

        if authentication_file:
            with open(authentication_file, 'rb') as f:
                token = f.read().strip()

            class TooltoolAuthenticator(HTTPAdapter):
                def send(self, request, *args, **kwargs):
                    request.headers['Authorization'] = \
                        'Bearer {}'.format(token)
                    return super(TooltoolAuthenticator, self).send(
                        request, *args, **kwargs)

            cache._download_manager.session.mount(
                tooltool_url, TooltoolAuthenticator())

        class DownloadRecord(FileRecord):
            def __init__(self, url, *args, **kwargs):
                super(DownloadRecord, self).__init__(*args, **kwargs)
                self.url = url
                self.basename = self.filename

            def fetch_with(self, cache):
                self.filename = cache.fetch(self.url)
                return self.filename

            def validate(self):
                if self.size is None and self.digest is None:
                    return True
                return super(DownloadRecord, self).validate()

        class ArtifactRecord(DownloadRecord):
            def __init__(self, task_id, artifact_name):
                for _ in redo.retrier(attempts=retry+1, sleeptime=60):
                    cot = cache._download_manager.session.get(
                        get_artifact_url(task_id, 'public/chainOfTrust.json.asc'))
                    if cot.status_code >= 500:
                        continue
                    cot.raise_for_status()
                    break
                else:
                    cot.raise_for_status()

                digest = algorithm = None
                data = {}
                # The file is GPG-signed, but we don't care about validating
                # that. Instead of parsing the PGP signature, we just take
                # the one line we're interested in, which starts with a `{`.
                for l in cot.content.splitlines():
                    if l.startswith('{'):
                        try:
                            data = json.loads(l)
                            break
                        except Exception:
                            pass
                for algorithm, digest in (data.get('artifacts', {})
                                              .get(artifact_name, {}).items()):
                    pass

                name = os.path.basename(artifact_name)
                artifact_url = get_artifact_url(task_id, artifact_name,
                    use_proxy=not artifact_name.startswith('public/'))
                super(ArtifactRecord, self).__init__(
                    artifact_url, name,
                    None, digest, algorithm, unpack=True)

        records = OrderedDict()
        downloaded = []

        if tooltool_manifest:
            manifest = open_manifest(tooltool_manifest)
            for record in manifest.file_records:
                url = '{}/{}/{}'.format(tooltool_url, record.algorithm,
                                        record.digest)
                records[record.filename] = DownloadRecord(
                    url, record.filename, record.size, record.digest,
                    record.algorithm, unpack=record.unpack,
                    version=record.version, visibility=record.visibility,
                    setup=record.setup)

        if from_build:
            if 'MOZ_AUTOMATION' in os.environ:
                self.log(logging.ERROR, 'artifact', {},
                         'Do not use --from-build in automation; all dependencies '
                         'should be determined in the decision task.')
                return 1
            from taskgraph.optimize import IndexSearch
            from taskgraph.parameters import Parameters
            from taskgraph.generator import load_tasks_for_kind
            params = Parameters(
                level=os.environ.get('MOZ_SCM_LEVEL', '3'),
                strict=False,
            )

            root_dir = mozpath.join(self.topsrcdir, 'taskcluster/ci')
            toolchains = load_tasks_for_kind(params, 'toolchain', root_dir=root_dir)

            aliases = {}
            for t in toolchains.values():
                alias = t.attributes.get('toolchain-alias')
                if alias:
                    aliases['toolchain-{}'.format(alias)] = \
                        t.task['metadata']['name']

            for b in from_build:
                user_value = b

                if not b.startswith('toolchain-'):
                    b = 'toolchain-{}'.format(b)

                task = toolchains.get(aliases.get(b, b))
                if not task:
                    self.log(logging.ERROR, 'artifact', {'build': user_value},
                             'Could not find a toolchain build named `{build}`')
                    return 1

                task_id = IndexSearch().should_replace_task(
                    task, {}, task.optimization.get('index-search', []))
                artifact_name = task.attributes.get('toolchain-artifact')
                if task_id in (True, False) or not artifact_name:
                    self.log(logging.ERROR, 'artifact', {'build': user_value},
                             'Could not find artifacts for a toolchain build '
                             'named `{build}`. Local commits and other changes '
                             'in your checkout may cause this error. Try '
                             'updating to a fresh checkout of mozilla-central '
                             'to use artifact builds.')
                    return 1

                record = ArtifactRecord(task_id, artifact_name)
                records[record.filename] = record

        # Handle the list of files of the form path@task-id on the command
        # line. Each of those give a path to an artifact to download.
        for f in files:
            if '@' not in f:
                self.log(logging.ERROR, 'artifact', {},
                         'Expected a list of files of the form path@task-id')
                return 1
            name, task_id = f.rsplit('@', 1)
            record = ArtifactRecord(task_id, name)
            records[record.filename] = record

        for record in records.itervalues():
            self.log(logging.INFO, 'artifact', {'name': record.basename},
                     'Downloading {name}')
            valid = False
            # sleeptime is 60 per retry.py, used by tooltool_wrapper.sh
            for attempt, _ in enumerate(redo.retrier(attempts=retry+1,
                                                     sleeptime=60)):
                try:
                    record.fetch_with(cache)
                except (requests.exceptions.HTTPError,
                        requests.exceptions.ChunkedEncodingError,
                        requests.exceptions.ConnectionError) as e:

                    if isinstance(e, requests.exceptions.HTTPError):
                        # The relengapi proxy likes to return error 400 bad request
                        # which seems improbably to be due to our (simple) GET
                        # being borked.
                        status = e.response.status_code
                        should_retry = status >= 500 or status == 400
                    else:
                        should_retry = True

                    if should_retry or attempt < retry:
                        level = logging.WARN
                    else:
                        level = logging.ERROR
                    # e.message is not always a string, so convert it first.
                    self.log(level, 'artifact', {}, str(e.message))
                    if not should_retry:
                        break
                    if attempt < retry:
                        self.log(logging.INFO, 'artifact', {},
                                 'Will retry in a moment...')
                    continue
                try:
                    valid = record.validate()
                except Exception:
                    pass
                if not valid:
                    os.unlink(record.filename)
                    if attempt < retry:
                        self.log(logging.INFO, 'artifact', {},
                                 'Will retry in a moment...')
                    continue

                downloaded.append(record)
                break

            if not valid:
                self.log(logging.ERROR, 'artifact', {'name': record.basename},
                         'Failed to download {name}')
                return 1

        artifacts = {} if artifact_manifest else None

        for record in downloaded:
            local = os.path.join(os.getcwd(), record.basename)
            if os.path.exists(local):
                os.unlink(local)
            # unpack_file needs the file with its final name to work
            # (https://github.com/mozilla/build-tooltool/issues/38), so we
            # need to copy it, even though we remove it later. Use hard links
            # when possible.
            try:
                os.link(record.filename, local)
            except Exception:
                shutil.copy(record.filename, local)
            # Keep a sha256 of each downloaded file, for the chain-of-trust
            # validation.
            if artifact_manifest is not None:
                with open(local) as fh:
                    h = hashlib.sha256()
                    while True:
                        data = fh.read(1024 * 1024)
                        if not data:
                            break
                        h.update(data)
                artifacts[record.url] = {
                    'sha256': h.hexdigest(),
                }
            if record.unpack and not no_unpack:
                unpack_file(local, record.setup)
                os.unlink(local)

        if not downloaded:
            self.log(logging.ERROR, 'artifact', {}, 'Nothing to download')
            if files:
                return 1

        if artifacts:
            ensureParentDir(artifact_manifest)
            with open(artifact_manifest, 'w') as fh:
                json.dump(artifacts, fh, indent=4, sort_keys=True)

        return 0

class StaticAnalysisSubCommand(SubCommand):
    def __call__(self, func):
        after = SubCommand.__call__(self, func)
        args = [
            CommandArgument('--verbose', '-v', action='store_true',
                            help='Print verbose output.'),
        ]
        for arg in args:
            after = arg(after)
        return after


class StaticAnalysisMonitor(object):
    def __init__(self, srcdir, objdir, total):
        self._total = total
        self._processed = 0
        self._current = None
        self._srcdir = srcdir

        from mozbuild.compilation.warnings import (
            WarningsCollector,
            WarningsDatabase,
        )

        self._warnings_database = WarningsDatabase()

        def on_warning(warning):
            filename = warning['filename']
            self._warnings_database.insert(warning)

        self._warnings_collector = WarningsCollector(on_warning, objdir=objdir)

    @property
    def num_files(self):
        return self._total

    @property
    def num_files_processed(self):
        return self._processed

    @property
    def current_file(self):
        return self._current

    @property
    def warnings_db(self):
        return self._warnings_database

    def on_line(self, line):
        warning = None

        try:
            warning = self._warnings_collector.process_line(line)
        except:
            pass

        if line.find('clang-tidy') != -1:
            filename = line.split(' ')[-1]
            if os.path.isfile(filename):
                self._current = os.path.relpath(filename, self._srcdir)
            else:
                self._current = None
            self._processed = self._processed + 1
            return (warning, False)
        return (warning, True)


@CommandProvider
class StaticAnalysis(MachCommandBase):
    """Utilities for running C++ static analysis checks and format."""

    # List of file extension to consider (should start with dot)
    _format_include_extensions = ('.cpp', '.c', '.cc', '.h', '.java')
    # File contaning all paths to exclude from formatting
    _format_ignore_file = '.clang-format-ignore'

    @Command('static-analysis', category='testing',
             description='Run C++ static analysis checks')
    def static_analysis(self):
        # If not arguments are provided, just print a help message.
        mach = Mach(os.getcwd())
        mach.run(['static-analysis', '--help'])

    @StaticAnalysisSubCommand('static-analysis', 'check',
                              'Run the checks using the helper tool')
    @CommandArgument('source', nargs='*', default=['.*'],
                     help='Source files to be analyzed (regex on path). '
                          'Can be omitted, in which case the entire code base '
                          'is analyzed.  The source argument is ignored if '
                          'there is anything fed through stdin, in which case '
                          'the analysis is only performed on the files changed '
                          'in the patch streamed through stdin.  This is called '
                          'the diff mode.')
    @CommandArgument('--checks', '-c', default='-*', metavar='checks',
                     help='Static analysis checks to enable.  By default, this enables only '
                     'checks that are published here: https://mzl.la/2DRHeTh, but can be any '
                     'clang-tidy checks syntax.')
    @CommandArgument('--jobs', '-j', default='0', metavar='jobs', type=int,
                     help='Number of concurrent jobs to run. Default is the number of CPUs.')
    @CommandArgument('--strip', '-p', default='1', metavar='NUM',
                     help='Strip NUM leading components from file names in diff mode.')
    @CommandArgument('--fix', '-f', default=False, action='store_true',
                     help='Try to autofix errors detected by clang-tidy checkers.')
    @CommandArgument('--header-filter', '-h-f', default='', metavar='header_filter',
                     help='Regular expression matching the names of the headers to '
                          'output diagnostics from. Diagnostics from the main file '
                          'of each translation unit are always displayed')
    def check(self, source=None, jobs=2, strip=1, verbose=False,
              checks='-*', fix=False, header_filter=''):
        from mozbuild.controller.building import (
            StaticAnalysisFooter,
            StaticAnalysisOutputManager,
        )

        self._set_log_level(verbose)
        self.log_manager.enable_all_structured_loggers()

        rc = self._build_compile_db(verbose=verbose)
        rc = rc or self._build_export(jobs=jobs, verbose=verbose)
        rc = rc or self._get_clang_tools(verbose=verbose)
        if rc != 0:
            return rc

        compile_db = json.loads(open(self._compile_db, 'r').read())
        total = 0
        import re
        name_re = re.compile('(' + ')|('.join(source) + ')')
        for f in compile_db:
            if name_re.search(f['file']):
                total = total + 1

        if not total:
            return 0

        cwd = self.topobjdir
        self._compilation_commands_path = self.topobjdir
        self._clang_tidy_config = self._get_clang_tidy_config()
        args = self._get_clang_tidy_command(
            checks=checks, header_filter=header_filter, sources=source, jobs=jobs, fix=fix)

        monitor = StaticAnalysisMonitor(self.topsrcdir, self.topobjdir, total)

        footer = StaticAnalysisFooter(self.log_manager.terminal, monitor)
        with StaticAnalysisOutputManager(self.log_manager, monitor, footer) as output:
            rc = self.run_process(args=args, ensure_exit_code=False, line_handler=output.on_line, cwd=cwd)

            self.log(logging.WARNING, 'warning_summary',
                     {'count': len(monitor.warnings_db)},
                     '{count} warnings present.')
        if rc != 0:
            return rc
        # if we are building firefox for android it might be nice to
        # also analyze the java code base
        if self.substs['MOZ_BUILD_APP'] == 'mobile/android':
            rc = self.check_java(source, jobs, strip, verbose, skip_export=True)
        return rc

    @StaticAnalysisSubCommand('static-analysis', 'check-java',
                              'Run infer on the java codebase.')
    @CommandArgument('source', nargs='*', default=['mobile'],
                     help='Source files to be analyzed. '
                          'Can be omitted, in which case the entire code base '
                          'is analyzed.  The source argument is ignored if '
                          'there is anything fed through stdin, in which case '
                          'the analysis is only performed on the files changed '
                          'in the patch streamed through stdin.  This is called '
                          'the diff mode.')
    @CommandArgument('--checks', '-c', default=[], metavar='checks', nargs='*',
                     help='Static analysis checks to enable.')
    @CommandArgument('--jobs', '-j', default='0', metavar='jobs', type=int,
                     help='Number of concurrent jobs to run.'
                     ' Default is the number of CPUs.')
    @CommandArgument('--task', '-t', type=str,
                     default='compileWithGeckoBinariesDebugSources',
                     help='Which gradle tasks to use to compile the java codebase.')
    def check_java(self, source=['mobile'], jobs=2, strip=1, verbose=False, checks=[],
                   task='compileWithGeckoBinariesDebugSources',
                   skip_export=False):
        self._set_log_level(verbose)
        self.log_manager.enable_all_structured_loggers()
        if self.substs['MOZ_BUILD_APP'] != 'mobile/android':
            self.log(logging.WARNING, 'static-analysis', {},
                     'Cannot check java source code unless you are building for android!')
            return 1
        rc = self._check_for_java()
        if rc != 0:
            return 1
        # if source contains the whole mobile folder, then we just have to
        # analyze everything
        check_all = any(i.rstrip(os.sep).split(os.sep)[-1] == 'mobile' for i in source)
        # gather all java sources from the source variable
        java_sources = []
        if not check_all:
            java_sources = self._get_java_files(source)
            if not java_sources:
                return 0
        if not skip_export:
            rc = self._build_export(jobs=jobs, verbose=verbose)
            if rc != 0:
                return rc
        rc = self._get_infer(verbose=verbose)
        if rc != 0:
            self.log(logging.WARNING, 'static-analysis', {},
                     'This command is only available for linux64!')
            return rc
        # which checkers to use, and which folders to exclude
        all_checkers, third_party_path = self._get_infer_config()
        checkers, excludes = self._get_infer_args(
            checks=checks or all_checkers,
            third_party_path=third_party_path
        )
        rc = rc or self._gradle(['clean'])  # clean so that we can recompile
        # infer capture command
        capture_cmd = [self._infer_path, 'capture'] + excludes + ['--']
        rc = rc or self._gradle([task], infer_args=capture_cmd, verbose=verbose)
        tmp_file, args = self._get_infer_source_args(java_sources)
        # infer analyze command
        analysis_cmd = [self._infer_path, 'analyze', '--keep-going'] +  \
            checkers + args
        rc = rc or self.run_process(args=analysis_cmd, cwd=self.topsrcdir, pass_thru=True)
        if tmp_file:
            tmp_file.close()
        return rc

    def _get_java_files(self, sources):
        java_sources = []
        for i in sources:
            f = mozpath.join(self.topsrcdir, i)
            if os.path.isdir(f):
                for root, dirs, files in os.walk(f):
                    dirs.sort()
                    for file in sorted(files):
                        if file.endswith('.java'):
                            java_sources.append(mozpath.join(root, file))
            elif f.endswith('.java'):
                java_sources.append(f)
        return java_sources

    def _get_infer_source_args(self, sources):
        '''Return the arguments to only analyze <sources>'''
        if not sources:
            return (None, [])
        # create a temporary file in which we place all sources
        # this is used by the analysis command to only analyze certain files
        f = tempfile.NamedTemporaryFile()
        for source in sources:
            f.write(source+'\n')
        f.flush()
        return (f, ['--changed-files-index', f.name])

    def _get_infer_config(self):
        '''Load the infer config file.'''
        checkers = []
        tp_path = ''
        with open(mozpath.join(self.topsrcdir, 'tools',
                               'infer', 'config.yaml')) as f:
            try:
                config = yaml.safe_load(f)
                for item in config['infer_checkers']:
                    if item['publish']:
                        checkers.append(item['name'])
                tp_path = mozpath.join(self.topsrcdir, config['third_party'])
            except Exception as e:
                print('Looks like config.yaml is not valid, so we are unable '
                      'to determine default checkers, and which folder to '
                      'exclude, using defaults provided by infer')
        return checkers, tp_path

    def _get_infer_args(self, checks, third_party_path):
        '''Return the arguments which include the checkers <checks>, and
        excludes all folder in <third_party_path>.'''
        checkers = ['-a', 'checkers']
        excludes = []
        for checker in checks:
            checkers.append('--' + checker)
        with open(third_party_path) as f:
            for line in f:
                excludes.append('--skip-analysis-in-path')
                excludes.append(line.strip('\n'))
        return checkers, excludes

    def _get_clang_tidy_config(self):
        try:
            file_handler = open(mozpath.join(self.topsrcdir, "tools", "clang-tidy", "config.yaml"))
            config = yaml.safe_load(file_handler)
        except Exception:
                print('Looks like config.yaml is not valid, we are going to use default'
                      ' values for the rest of the analysis.')
                return None
        return config

    def _get_clang_tidy_command(self, checks, header_filter, sources, jobs, fix):

        if checks == '-*':
            checks = self._get_checks()

        common_args = ['-clang-tidy-binary', self._clang_tidy_path,
                       '-clang-apply-replacements-binary', self._clang_apply_replacements,
                       '-checks=%s' % checks,
                       '-extra-arg=-DMOZ_CLANG_PLUGIN']

        # Flag header-filter is passed in order to limit the diagnostic messages only
        # to the specified header files. When no value is specified the default value
        # is considered to be the source in order to limit the diagnostic message to
        # the source files or folders.
        common_args += ['-header-filter=%s' % (header_filter
                                              if len(header_filter) else '|'.join(sources))]

        # From our configuration file, config.yaml, we build the configuration list, for
        # the checkers that are used. These configuration options are used to better fit
        # the checkers to our code.
        cfg = self._get_checks_config()
        if cfg:
            common_args += ['-config=%s' % yaml.dump(cfg)]

        if fix:
            common_args += '-fix'

        return [
            self.virtualenv_manager.python_path, self._run_clang_tidy_path, '-j',
            str(jobs), '-p', self._compilation_commands_path
        ] + common_args + sources

    def _check_for_java(self):
        '''Check if javac can be found.'''
        import distutils
        java = self.substs.get('JAVA')
        java = java or os.getenv('JAVA_HOME')
        java = java or distutils.spawn.find_executable('javac')
        error = 'javac was not found! Please install javac and either add it to your PATH, '
        error += 'set JAVA_HOME, or add the following to your mozconfig:\n'
        error += '  --with-java-bin-path=/path/to/java/bin/'
        if not java:
            self.log(logging.ERROR, 'ERROR: static-analysis', {}, error)
            return 1
        return 0

    def _gradle(self, args, infer_args=None, verbose=False, autotest=False,
                suppress_output=True):
        infer_args = infer_args or []
        if autotest:
            cwd = mozpath.join(self.topsrcdir, 'tools', 'infer', 'test')
            gradle = mozpath.join(cwd, 'gradlew')
        else:
            gradle = self.substs['GRADLE']
            cwd = self.topsrcdir
        extra_env = {
            'GRADLE_OPTS': '-Dfile.encoding=utf-8',  # see mobile/android/mach_commands.py
            'JAVA_TOOL_OPTIONS': '-Dfile.encoding=utf-8',
        }
        if suppress_output:
            devnull = open(os.devnull, 'w')
            return subprocess.call(
                infer_args + [gradle] + args,
                env=dict(os.environ, **extra_env),
                cwd=cwd, stdout=devnull, stderr=subprocess.STDOUT, close_fds=True)
        else:
            return self.run_process(
                infer_args + [gradle] + args,
                append_env=extra_env,
                pass_thru=True,  # Allow user to run gradle interactively.
                ensure_exit_code=False,  # Don't throw on non-zero exit code.
                cwd=cwd)

    @StaticAnalysisSubCommand('static-analysis', 'autotest',
                              'Run the auto-test suite in order to determine that'
                              ' the analysis did not regress.')
    @CommandArgument('--dump-results', '-d', default=False, action='store_true',
                     help='Generate the baseline for the regression test. Based on'
                     ' this baseline we will test future results.')
    @CommandArgument('--intree-tool', '-i', default=False, action='store_true',
                     help='Use a pre-aquired in-tree clang-tidy package.')
    @CommandArgument('checker_names', nargs='*', default=[],
                     help='Checkers that are going to be auto-tested.')
    def autotest(self, verbose=False, dump_results=False, intree_tool=False, checker_names=[]):
        # If 'dump_results' is True than we just want to generate the issues files for each
        # checker in particulat and thus 'force_download' becomes 'False' since we want to
        # do this on a local trusted clang-tidy package.
        self._set_log_level(verbose)
        self._dump_results = dump_results

        force_download = not self._dump_results

        # Function return codes
        self.TOOLS_SUCCESS = 0
        self.TOOLS_FAILED_DOWNLOAD = 1
        self.TOOLS_UNSUPORTED_PLATFORM = 2
        self.TOOLS_CHECKER_NO_TEST_FILE = 3
        self.TOOLS_CHECKER_RETURNED_NO_ISSUES = 4
        self.TOOLS_CHECKER_RESULT_FILE_NOT_FOUND = 5
        self.TOOLS_CHECKER_DIFF_FAILED = 6
        self.TOOLS_CHECKER_NOT_FOUND = 7
        self.TOOLS_CHECKER_FAILED_FILE = 8
        self.TOOLS_CHECKER_LIST_EMPTY = 9
        self.TOOLS_GRADLE_FAILED = 10

        # Configure the tree or download clang-tidy package, depending on the option that we choose
        if intree_tool:
            _, config, _ = self._get_config_environment()
            clang_tools_path = self.topsrcdir
            self._clang_tidy_path = mozpath.join(
                clang_tools_path, "clang-tidy", "bin",
                "clang-tidy" + config.substs.get('BIN_SUFFIX', ''))
            self._clang_format_path = mozpath.join(
                clang_tools_path, "clang-tidy", "bin",
                "clang-format" + config.substs.get('BIN_SUFFIX', ''))
            self._clang_apply_replacements = mozpath.join(
                clang_tools_path, "clang-tidy", "bin",
                "clang-apply-replacements" + config.substs.get('BIN_SUFFIX', ''))
            self._run_clang_tidy_path = mozpath.join(clang_tools_path, "clang-tidy", "share",
                                                     "clang", "run-clang-tidy.py")
            self._clang_format_diff = mozpath.join(clang_tools_path, "clang-tidy", "share",
                                                   "clang", "clang-format-diff.py")

            # Ensure that clang-tidy is present
            rc = not os.path.exists(self._clang_tidy_path)
        else:
            rc = self._get_clang_tools(force=force_download, verbose=verbose)

        if rc != 0:
            self.log(logging.ERROR, 'ERROR: static-analysis', {},
                     'clang-tidy unable to locate package.')
            return self.TOOLS_FAILED_DOWNLOAD

        self._clang_tidy_base_path = mozpath.join(self.topsrcdir, "tools", "clang-tidy")

        # For each checker run it
        self._clang_tidy_config = self._get_clang_tidy_config()
        platform, _ = self.platform

        if platform not in self._clang_tidy_config['platforms']:
            self.log(logging.ERROR, 'static-analysis', {},
                     "RUNNING: clang-tidy autotest for platform {} not supported.".format(platform))
            return self.TOOLS_UNSUPORTED_PLATFORM

        import concurrent.futures
        import multiprocessing
        import shutil

        max_workers = multiprocessing.cpu_count()

        self.log(logging.INFO, 'static-analysis', {},
                 "RUNNING: clang-tidy autotest for platform {0} with {1} workers.".format(
                     platform, max_workers))

        # List all available checkers
        cmd = [self._clang_tidy_path, '-list-checks', '-checks=*']
        clang_output = subprocess.check_output(
            cmd, stderr=subprocess.STDOUT).decode('utf-8')
        available_checks = clang_output.split('\n')[1:]
        self._clang_tidy_checks = [c.strip() for c in available_checks if c]

        # Build the dummy compile_commands.json
        self._compilation_commands_path = self._create_temp_compilation_db(self._clang_tidy_config)
        checkers_test_batch = []
        with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = []
            for item in self._clang_tidy_config['clang_checkers']:
                # Skip if any of the following statements is true:
                # 1. Checker attribute 'publish' is False.
                not_published = not bool(item.get('publish', True))
                # 2. Checker has restricted-platforms and current platform is not of them.
                ignored_platform = 'restricted-platforms' in item and platform not in item['restricted-platforms']
                # 3. Checker name is mozilla-* or -*.
                ignored_checker = item['name'] in ['mozilla-*', '-*']
                # 4. List checker_names is passed and the current checker is not part of the
                #    list or 'publish' is False
                checker_not_in_list = checker_names and (item['name'] not in checker_names or not_published)
                if not_published or \
                   ignored_platform or \
                   ignored_checker or \
                   checker_not_in_list:
                    continue
                checkers_test_batch.append(item['name'])
                futures.append(executor.submit(self._verify_checker, item))

            error_code = self.TOOLS_SUCCESS
            for future in concurrent.futures.as_completed(futures):
                # Wait for every task to finish
                ret_val = future.result()
                if ret_val != self.TOOLS_SUCCESS:
                    # We are interested only in one error and we don't break
                    # the execution of for loop since we want to make sure that all
                    # tasks finished.
                    error_code = ret_val

            if error_code != self.TOOLS_SUCCESS:
                self.log(logging.INFO, 'static-analysis', {}, "FAIL: clang-tidy some tests failed.")
                # Also delete the tmp folder
                shutil.rmtree(self._compilation_commands_path)
                return error_code

            # Run the analysis on all checkers at the same time only if we don't dump results.
            if not self._dump_results:
                ret_val = self._run_analysis_batch(checkers_test_batch)
                if ret_val != self.TOOLS_SUCCESS:
                    shutil.rmtree(self._compilation_commands_path)
                    return ret_val

        self.log(logging.INFO, 'static-analysis', {}, "SUCCESS: clang-tidy all tests passed.")
        # Also delete the tmp folder
        shutil.rmtree(self._compilation_commands_path)
        return self._autotest_infer(intree_tool, force_download, verbose)

    def _run_analysis(self, checks, header_filter, sources, jobs=1, fix=False, print_out=False):
        cmd = self._get_clang_tidy_command(
            checks=checks, header_filter=header_filter,
            sources=sources,
            jobs=jobs, fix=fix)

        try:
            clang_output = subprocess.check_output(cmd, stderr=subprocess.STDOUT).decode('utf-8')
        except subprocess.CalledProcessError as e:
            print(e.output)
            return None
        return self._parse_issues(clang_output)

    def _run_analysis_batch(self, items):
        self.log(logging.INFO, 'static-analysis', {},"RUNNING: clang-tidy checker batch analysis.")
        if not len(items):
            self.log(logging.ERROR, 'static-analysis', {}, "ERROR: clang-tidy checker list is empty!")
            return self.TOOLS_CHECKER_LIST_EMPTY

        issues = self._run_analysis(
            checks='-*,' + ",".join(items), header_filter='',
            sources=[mozpath.join(self._clang_tidy_base_path, "test", checker) + '.cpp' for checker in items], print_out=True)

        if issues is None:
            return self.TOOLS_CHECKER_FAILED_FILE

        for checker in items:
            test_file_path_json = mozpath.join(self._clang_tidy_base_path, "test", checker) + '.json'
             # Read the pre-determined issues
            baseline_issues = self._get_autotest_stored_issues(test_file_path_json)
            found = all([element_base in issues for element_base in baseline_issues])

            if not found:
                self.log(
                    logging.ERROR, 'static-analysis', {},
                    "ERROR: clang-tidy auto-test failed for checker {0} in multiple files process unit.".
                    format(checker))
                return self.TOOLS_CHECKER_DIFF_FAILED
        return self.TOOLS_SUCCESS

    def _create_temp_compilation_db(self, config):
        directory = tempfile.mkdtemp(prefix='cc')
        with open(mozpath.join(directory, "compile_commands.json"), "wb") as file_handler:
            compile_commands = []
            director = mozpath.join(self.topsrcdir, 'tools', 'clang-tidy', 'test')
            for item in config['clang_checkers']:
                if item['name'] in ['-*', 'mozilla-*']:
                    continue
                file = item['name'] + '.cpp'
                element = {}
                element["directory"] = director
                element["command"] = 'cpp ' + file
                element["file"] = mozpath.join(director, file)
                compile_commands.append(element)

            json.dump(compile_commands, file_handler)
            file_handler.flush()

            return directory

    def _autotest_infer(self, intree_tool, force_download, verbose):
        # infer is not available on other platforms, but autotest should work even without
        # it being installed
        if self.platform[0] == 'linux64':
            rc = self._check_for_java()
            if rc != 0:
                return 1
            rc = self._get_infer(force=force_download, verbose=verbose, intree_tool=intree_tool)
            if rc != 0:
                self.log(logging.ERROR, 'ERROR: static-analysis', {},
                         'infer unable to locate package.')
                return self.TOOLS_FAILED_DOWNLOAD
            self.__infer_tool = mozpath.join(self.topsrcdir, 'tools', 'infer')
            self.__infer_test_folder = mozpath.join(self.__infer_tool, 'test')

            import concurrent.futures
            import multiprocessing
            max_workers = multiprocessing.cpu_count()
            self.log(logging.INFO, 'static-analysis', {},
                     "RUNNING: infer autotest for platform {0} with {1} workers.".format(
                         self.platform[0], max_workers))
            # clean previous autotest if it exists
            rc = self._gradle(['autotest:clean'], autotest=True)
            if rc != 0:
                return rc
            import yaml
            with open(mozpath.join(self.__infer_tool, 'config.yaml')) as f:
                config = yaml.safe_load(f)
            with concurrent.futures.ThreadPoolExecutor(max_workers=max_workers) as executor:
                futures = []
                for item in config['infer_checkers']:
                    if item['publish']:
                        futures.append(executor.submit(self._verify_infer_checker, item))
                # this is always included in check-java, but not in config.yaml
                futures.append(executor.submit(self._verify_infer_checker,
                                               {'name': 'checkers'}))
                for future in concurrent.futures.as_completed(futures):
                    ret_val = future.result()
                    if ret_val != self.TOOLS_SUCCESS:
                        return ret_val
            self.log(logging.INFO, 'static-analysis', {}, "SUCCESS: infer all tests passed.")
        else:
            self.log(logging.WARNING, 'static-analysis', {},
                     "Skipping infer autotest, because it is only available on linux64!")
        return self.TOOLS_SUCCESS

    def _verify_infer_checker(self, item):
        '''Given a checker, this method verifies the following:
          1. if there is a `checker`.json and `checker`.java file in
             `tools/infer/test/autotest/src`
          2. if running infer on `checker`.java yields the same result as `checker`.json
        An `item` is simply a dictionary, which needs to have a `name` field set, which is the
        name of the checker.
        '''
        def to_camelcase(str):
            return ''.join([s.capitalize() for s in str.split('-')])
        check = item['name']
        test_file_path = mozpath.join(self.__infer_tool, 'test', 'autotest', 'src',
                                      'main', 'java', to_camelcase(check))
        test_file_path_java = test_file_path + '.java'
        test_file_path_json = test_file_path + '.json'
        self.log(logging.INFO, 'static-analysis', {}, "RUNNING: infer check {}.".format(check))
        # Verify if the test file exists for this checker
        if not os.path.exists(test_file_path_java):
            self.log(logging.ERROR, 'static-analysis', {},
                     "ERROR: infer check {} doesn't have a test file.".format(check))
            return self.TOOLS_CHECKER_NO_TEST_FILE
        # run infer on a particular test file
        out_folder = mozpath.join(self.__infer_test_folder, 'test-infer-{}'.format(check))
        if check == 'checkers':
            check_arg = ['-a', 'checkers']
        else:
            check_arg = ['--{}-only'.format(check)]
        infer_args = [self._infer_path, 'run'] + check_arg + ['-o', out_folder, '--']
        gradle_args = ['autotest:compileInferTest{}'.format(to_camelcase(check))]
        rc = self._gradle(gradle_args, infer_args=infer_args, autotest=True)
        if rc != 0:
            self.log(logging.ERROR, 'static-analysis', {},
                     "ERROR: infer failed to execute gradle {}.".format(gradle_args))
            return self.TOOLS_GRADLE_FAILED
        issues = json.load(open(mozpath.join(out_folder, 'report.json')))
        # remove folder that infer creates because the issues are loaded into memory
        import shutil
        shutil.rmtree(out_folder)
        # Verify to see if we got any issues, if not raise exception
        if not issues:
            self.log(
                logging.ERROR, 'static-analysis', {},
                "ERROR: infer check {0} did not find any issues in its associated test suite."
                .format(check)
            )
            return self.TOOLS_CHECKER_RETURNED_NO_ISSUES
        if self._dump_results:
            self._build_autotest_result(test_file_path_json, issues)
        else:
            if not os.path.exists(test_file_path_json):
                # Result file for test not found maybe regenerate it?
                self.log(
                    logging.ERROR, 'static-analysis', {},
                    "ERROR: infer result file not found for check {0}".format(check)
                )
                return self.TOOLS_CHECKER_RESULT_FILE_NOT_FOUND
            # Read the pre-determined issues
            baseline_issues = self._get_autotest_stored_issues(test_file_path_json)

            def ordered(obj):
                if isinstance(obj, dict):
                    return sorted((k, ordered(v)) for k, v in obj.items())
                if isinstance(obj, list):
                    return sorted(ordered(x) for x in obj)
                return obj
            # Compare the two lists
            if ordered(issues) != ordered(baseline_issues):
                error_str = "ERROR: in check {} Expected: ".format(check)
                error_str += '\n' + json.dumps(baseline_issues, indent=2)
                error_str += '\n Got:\n' + json.dumps(issues, indent=2)
                self.log(logging.ERROR, 'static-analysis', {},
                         'ERROR: infer autotest for check {} failed, check stdout for more details'
                         .format(check))
                print(error_str)
                return self.TOOLS_CHECKER_DIFF_FAILED
        return self.TOOLS_SUCCESS

    @StaticAnalysisSubCommand('static-analysis', 'install',
                              'Install the static analysis helper tool')
    @CommandArgument('source', nargs='?', type=str,
                     help='Where to fetch a local archive containing the static-analysis and '
                     'format helper tool.'
                          'It will be installed in ~/.mozbuild/clang-tools and ~/.mozbuild/infer.'
                          'Can be omitted, in which case the latest clang-tools and infer '
                          'helper for the platform would be automatically detected and installed.')
    @CommandArgument('--skip-cache', action='store_true',
                     help='Skip all local caches to force re-fetching the helper tool.',
                     default=False)
    @CommandArgument('--force', action='store_true',
                     help='Force re-install even though the tool exists in mozbuild.',
                     default=False)
    @CommandArgument('--minimal-install', action='store_true', help='Download only clang based tool.',
                     default=False)
    def install(self, source=None, skip_cache=False, force=False, minimal_install=False, verbose=False):
        self._set_log_level(verbose)
        rc = self._get_clang_tools(force=force, skip_cache=skip_cache,
                                   source=source, verbose=verbose)
        if rc == 0 and not minimal_install:
            # XXX ignore the return code because if it fails or not, infer is
            # not mandatory, but clang-tidy is
            self._get_infer(force=force, skip_cache=skip_cache, verbose=verbose)
        return rc

    @StaticAnalysisSubCommand('static-analysis', 'clear-cache',
                              'Delete local helpers and reset static analysis helper tool cache')
    def clear_cache(self, verbose=False):
        self._set_log_level(verbose)
        rc = self._get_clang_tools(force=True, download_if_needed=True, skip_cache=True,
                                   verbose=verbose)
        if rc == 0:
            self._get_infer(force=True, download_if_needed=True, skip_cache=True,
                            verbose=verbose)
        if rc != 0:
            return rc
        return self._artifact_manager.artifact_clear_cache()

    @StaticAnalysisSubCommand('static-analysis', 'print-checks',
                              'Print a list of the static analysis checks performed by default')
    def print_checks(self, verbose=False):
        self._set_log_level(verbose)
        rc = self._get_clang_tools(verbose=verbose)
        if rc == 0:
            rc = self._get_infer(verbose=verbose)
        if rc != 0:
            return rc
        args = [self._clang_tidy_path, '-list-checks', '-checks=%s' % self._get_checks()]
        rc = self._run_command_in_objdir(args=args, pass_thru=True)
        if rc != 0:
            return rc
        checkers, _ = self._get_infer_config()
        print('Infer checks:')
        for checker in checkers:
            print(' '*4 + checker)
        return 0

    @Command('clang-format',  category='misc', description='Run clang-format on current changes')
    @CommandArgument('--show', '-s', action='store_true', default=False,
                     help='Show diff output on instead of applying changes')
    @CommandArgument('--assume-filename', '-a', nargs=1, default=None,
                     help='This option is usually used in the context of hg-formatsource.'
                          'When reading from stdin, clang-format assumes this '
                          'filename to look for a style config file (with '
                          '-style=file) and to determine the language. When '
                          'specifying this option only one file should be used '
                          'as an input and the output will be forwarded to stdin. '
                          'This option also impairs the download of the clang-tools '
                          'and assumes the package is already located in it\'s default '
                          'location')
    @CommandArgument('--path', '-p', nargs='+', default=None,
                     help='Specify the path(s) to reformat')
    def clang_format(self, show, assume_filename, path, verbose=False):
        # Run clang-format or clang-format-diff on the local changes
        # or files/directories
        if path is not None:
            path = self._conv_to_abspath(path)

        os.chdir(self.topsrcdir)

        # With assume_filename we want to have stdout clean since the result of the
        # format will be redirected to stdout. Only in case of errror we
        # write something to stdout.
        # We don't actually want to get the clang-tools here since we want in some
        # scenarios to do this in parallel so we relay on the fact that the tools
        # have already been downloaded via './mach bootstrap' or directly via
        # './mach static-analysis install'
        if assume_filename:
            rc = self._set_clang_tools_paths()
            if rc != 0:
                print("clang-format: Unable to set path to clang-format tools.")
                return rc

            if not self._do_clang_tools_exist():
                print("clang-format: Unable to set locate clang-format tools.")
                return 1
        else:
            rc = self._get_clang_tools(verbose=verbose)
            if rc != 0:
                return rc

        if path is None:
            return self._run_clang_format_diff(self._clang_format_diff,
                                               self._clang_format_path, show)
        else:
            if assume_filename:
                return self._run_clang_format_in_console(self._clang_format_path, path, assume_filename)

        return self._run_clang_format_path(self._clang_format_path, show, path)

    def _verify_checker(self, item):
        check = item['name']
        test_file_path = mozpath.join(self._clang_tidy_base_path, "test", check)
        test_file_path_cpp = test_file_path + '.cpp'
        test_file_path_json = test_file_path + '.json'

        self.log(logging.INFO, 'static-analysis', {},
                 "RUNNING: clang-tidy checker {}.".format(check))

        # Verify if this checker actually exists
        if not check in self._clang_tidy_checks:
            self.log(logging.ERROR, 'static-analysis', {}, "ERROR: clang-tidy checker {} doesn't exist in this clang-tidy version.".format(check))
            return self.TOOLS_CHECKER_NOT_FOUND

        # Verify if the test file exists for this checker
        if not os.path.exists(test_file_path_cpp):
            self.log(logging.ERROR, 'static-analysis', {}, "ERROR: clang-tidy checker {} doesn't have a test file.".format(check))
            return self.TOOLS_CHECKER_NO_TEST_FILE

        issues = self._run_analysis(
            checks='-*,' + check, header_filter='', sources=[test_file_path_cpp])
        if issues is None:
            return self.TOOLS_CHECKER_FAILED_FILE

        # Verify to see if we got any issues, if not raise exception
        if not issues:
            self.log(
                logging.ERROR, 'static-analysis', {},
                "ERROR: clang-tidy checker {0} did not find any issues in its associated test file.".
                format(check))
            return self.TOOLS_CHECKER_RETURNED_NO_ISSUES

        if self._dump_results:
            self._build_autotest_result(test_file_path_json, json.dumps(issues))
        else:
            if not os.path.exists(test_file_path_json):
                # Result file for test not found maybe regenerate it?
                self.log(
                    logging.ERROR, 'static-analysis', {},
                    "ERROR: clang-tidy result file not found for checker {0}".format(
                        check))
                return self.TOOLS_CHECKER_RESULT_FILE_NOT_FOUND
            # Read the pre-determined issues
            baseline_issues = self._get_autotest_stored_issues(test_file_path_json)

            # Compare the two lists
            if issues != baseline_issues:
                self.log(
                    logging.ERROR, 'static-analysis', {},
                    "ERROR: clang-tidy auto-test failed for checker {0} Expected: {1} Got: {2}".
                    format(check, baseline_issues, issues))
                return self.TOOLS_CHECKER_DIFF_FAILED
        return self.TOOLS_SUCCESS

    def _build_autotest_result(self, file, issues):
        with open(file, 'w') as f:
            f.write(issues)

    def _get_autotest_stored_issues(self, file):
        with open(file) as f:
            return json.load(f)

    def _parse_issues(self, clang_output):
        '''
        Parse clang-tidy output into structured issues
        '''

        # Limit clang output parsing to 'Enabled checks:'
        end = re.search(r'^Enabled checks:\n', clang_output, re.MULTILINE)
        if end is not None:
            clang_output = clang_output[:end.start()-1]

        # Sort headers by positions
        regex_header = re.compile(
            r'(.+):(\d+):(\d+): (warning|error): ([^\[\]\n]+)(?: \[([\.\w-]+)\])?$', re.MULTILINE)

        something = regex_header.finditer(clang_output)
        headers = sorted(
            regex_header.finditer(clang_output),
            key=lambda h: h.start()
        )

        issues = []
        for _, header in enumerate(headers):
            header_group = header.groups()
            element = [header_group[3], header_group[4], header_group[5]]
            issues.append(element)
        return issues

    def _get_checks(self):
        checks = '-*'
        try:
            config = self._clang_tidy_config
            for item in config['clang_checkers']:
                if item.get('publish', True):
                    checks += ',' + item['name']
        except Exception:
            print('Looks like config.yaml is not valid, so we are unable to '
                    'determine default checkers, using \'-checks=-*,mozilla-*\'')
            checks += ',mozilla-*'
        finally:
            return checks

    def _get_checks_config(self):
        config_list = []
        checker_config = {}
        try:
            config = self._clang_tidy_config
            for checker in config['clang_checkers']:
                if checker.get('publish', True) and 'config' in checker:
                    for checker_option in checker['config']:
                        # Verify if the format of the Option is correct,
                        # possibilities are:
                        # 1. CheckerName.Option
                        # 2. Option -> that will become CheckerName.Option
                        if not checker_option['key'].startswith(checker['name']):
                            checker_option['key'] = "{}.{}".format(
                                checker['name'], checker_option['key'])
                    config_list += checker['config']
            checker_config['CheckOptions'] = config_list
        except Exception:
            print('Looks like config.yaml is not valid, so we are unable to '
                    'determine configuration for checkers, so using default')
            checker_config = None
        finally:
            return checker_config

    def _get_config_environment(self):
        ran_configure = False
        config = None
        builder = Build(self._mach_context)

        try:
            config = self.config_environment
        except Exception:
            print('Looks like configure has not run yet, running it now...')
            rc = builder.configure()
            if rc != 0:
                return (rc, config, ran_configure)
            ran_configure = True
            try:
                config = self.config_environment
            except Exception as e:
                pass

        return (0, config, ran_configure)

    def _build_compile_db(self, verbose=False):
        self._compile_db = mozpath.join(self.topobjdir, 'compile_commands.json')
        if os.path.exists(self._compile_db):
            return 0
        else:
            rc, config, ran_configure = self._get_config_environment()
            if rc != 0:
                return rc

            if ran_configure:
                # Configure may have created the compilation database if the
                # mozconfig enables building the CompileDB backend by default,
                # So we recurse to see if the file exists once again.
                return self._build_compile_db(verbose=verbose)

            if config:
                print('Looks like a clang compilation database has not been '
                      'created yet, creating it now...')
                builder = Build(self._mach_context)
                rc = builder.build_backend(['CompileDB'], verbose=verbose)
                if rc != 0:
                    return rc
                assert os.path.exists(self._compile_db)
                return 0

    def _build_export(self, jobs, verbose=False):
        def on_line(line):
            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')

        builder = Build(self._mach_context)
        # First install what we can through install manifests.
        rc = builder._run_make(directory=self.topobjdir, target='pre-export',
                               line_handler=None, silent=not verbose)
        if rc != 0:
            return rc

        # Then build the rest of the build dependencies by running the full
        # export target, because we can't do anything better.
        return builder._run_make(directory=self.topobjdir, target='export',
                                 line_handler=None, silent=not verbose,
                                 num_jobs=jobs)

    def _conv_to_abspath(self, paths):
        # Converts all the paths to absolute pathnames
        tmp_path = []
        for f in paths:
            tmp_path.append(os.path.abspath(f))
        return tmp_path

    def _set_clang_tools_paths(self):
        rc, config, _ = self._get_config_environment()

        if rc != 0:
            return rc

        self._clang_tools_path = mozpath.join(self._mach_context.state_dir, "clang-tools")
        self._clang_tidy_path = mozpath.join(self._clang_tools_path, "clang-tidy", "bin",
                                             "clang-tidy" + config.substs.get('BIN_SUFFIX', ''))
        self._clang_format_path = mozpath.join(
            self._clang_tools_path, "clang-tidy", "bin",
            "clang-format" + config.substs.get('BIN_SUFFIX', ''))
        self._clang_apply_replacements = mozpath.join(
            self._clang_tools_path, "clang-tidy", "bin",
            "clang-apply-replacements" + config.substs.get('BIN_SUFFIX', ''))
        self._run_clang_tidy_path = mozpath.join(self._clang_tools_path, "clang-tidy", "share", "clang",
                                                 "run-clang-tidy.py")
        self._clang_format_diff = mozpath.join(self._clang_tools_path, "clang-tidy", "share", "clang",
                                               "clang-format-diff.py")
        return 0

    def _do_clang_tools_exist(self):
        return os.path.exists(self._clang_tidy_path) and \
               os.path.exists(self._clang_format_path) and \
               os.path.exists(self._clang_apply_replacements) and \
               os.path.exists(self._run_clang_tidy_path)

    def _get_clang_tools(self, force=False, skip_cache=False,
                         source=None, download_if_needed=True,
                         verbose=False):

        rc = self._set_clang_tools_paths()

        if rc != 0:
            return rc

        if self._do_clang_tools_exist() and not force:
            return 0

        if os.path.isdir(self._clang_tools_path) and download_if_needed:
            # The directory exists, perhaps it's corrupted?  Delete it
            # and start from scratch.
            import shutil
            shutil.rmtree(self._clang_tools_path)
            return self._get_clang_tools(force=force, skip_cache=skip_cache,
                                            source=source, verbose=verbose,
                                            download_if_needed=download_if_needed)

        # Create base directory where we store clang binary
        os.mkdir(self._clang_tools_path)

        if source:
            return self._get_clang_tools_from_source(source)

        self._artifact_manager = PackageFrontend(self._mach_context)

        if not download_if_needed:
            return 0

        job, _ = self.platform

        if job is None:
            raise Exception('The current platform isn\'t supported. '
                            'Currently only the following platforms are '
                            'supported: win32/win64, linux64 and macosx64.')
        else:
            job += '-clang-tidy'

        # We want to unpack data in the clang-tidy mozbuild folder
        currentWorkingDir = os.getcwd()
        os.chdir(self._clang_tools_path)
        rc = self._artifact_manager.artifact_toolchain(verbose=verbose,
                                                        skip_cache=skip_cache,
                                                        from_build=[job],
                                                        no_unpack=False,
                                                        retry=0)
        # Change back the cwd
        os.chdir(currentWorkingDir)

        return rc

    def _get_clang_tools_from_source(self, filename):
        from mozbuild.action.tooltool import unpack_file
        clang_tidy_path = mozpath.join(self._mach_context.state_dir,
                                       "clang-tools")

        currentWorkingDir = os.getcwd()
        os.chdir(clang_tidy_path)

        unpack_file(filename)

        # Change back the cwd
        os.chdir(currentWorkingDir)

        clang_path = mozpath.join(clang_tidy_path, 'clang')

        if not os.path.isdir(clang_path):
            raise Exception('Extracted the archive but didn\'t find '
                            'the expected output')

        assert os.path.exists(self._clang_tidy_path)
        assert os.path.exists(self._clang_format_path)
        assert os.path.exists(self._clang_apply_replacements)
        assert os.path.exists(self._run_clang_tidy_path)
        return 0

    def _get_clang_format_diff_command(self):
        if self.repository.name == 'hg':
            args = ["hg", "diff", "-U0", "-r" ".^"]
            for dot_extension in self._format_include_extensions:
                args += ['--include', 'glob:**{0}'.format(dot_extension)]
            args += ['--exclude', 'listfile:{0}'.format(self._format_ignore_file)]
        else:
            args = ["git", "diff", "--no-color", "-U0", "HEAD", "--"]
            for dot_extension in self._format_include_extensions:
                args += ['*{0}'.format(dot_extension)]
            # git-diff doesn't support an 'exclude-from-files' param, but
            # allow to add individual exclude pattern since v1.9, see
            # https://git-scm.com/docs/gitglossary#gitglossary-aiddefpathspecapathspec
            with open(self._format_ignore_file, 'rb') as exclude_pattern_file:
                for pattern in exclude_pattern_file.readlines():
                    pattern = pattern.rstrip()
                    pattern = pattern.replace('.*', '**')
                    if not pattern or pattern.startswith('#'):
                        continue  # empty or comment
                    magics = ['exclude']
                    if pattern.startswith('^'):
                        magics += ['top']
                        pattern = pattern[1:]
                    args += [':({0}){1}'.format(','.join(magics), pattern)]
        return args

    def _get_infer(self, force=False, skip_cache=False, download_if_needed=True,
                   verbose=False, intree_tool=False):
        rc, config, _ = self._get_config_environment()
        if rc != 0:
            return rc
        infer_path = self.topsrcdir if intree_tool else \
            mozpath.join(self._mach_context.state_dir, 'infer')
        self._infer_path = mozpath.join(infer_path, 'infer', 'bin', 'infer' +
                                        config.substs.get('BIN_SUFFIX', ''))
        if intree_tool:
            return not os.path.exists(self._infer_path)
        if os.path.exists(self._infer_path) and not force:
            return 0
        else:
            if os.path.isdir(infer_path) and download_if_needed:
                # The directory exists, perhaps it's corrupted?  Delete it
                # and start from scratch.
                import shutil
                shutil.rmtree(infer_path)
                return self._get_infer(force=force, skip_cache=skip_cache,
                                       verbose=verbose,
                                       download_if_needed=download_if_needed)
            os.mkdir(infer_path)
            self._artifact_manager = PackageFrontend(self._mach_context)
            if not download_if_needed:
                return 0
            job, _ = self.platform
            if job != 'linux64':
                return -1
            else:
                job += '-infer'
            # We want to unpack data in the infer mozbuild folder
            currentWorkingDir = os.getcwd()
            os.chdir(infer_path)
            rc = self._artifact_manager.artifact_toolchain(verbose=verbose,
                                                           skip_cache=skip_cache,
                                                           from_build=[job],
                                                           no_unpack=False,
                                                           retry=0)
            # Change back the cwd
            os.chdir(currentWorkingDir)
            return rc

    def _run_clang_format_diff(self, clang_format_diff, clang_format, show):
        # Run clang-format on the diff
        # Note that this will potentially miss a lot things
        from subprocess import Popen, PIPE, check_output, CalledProcessError

        diff_process = Popen(self._get_clang_format_diff_command(), stdout=PIPE)
        args = [sys.executable, clang_format_diff, "-p1", "-binary=%s" % clang_format]

        if not show:
            args.append("-i")
        try:
            output = check_output(args, stdin=diff_process.stdout)
            if show:
                # We want to print the diffs
                print(output)
            return 0
        except CalledProcessError as e:
            # Something wrong happend
            print("clang-format: An error occured while running clang-format-diff.")
            return e.returncode

    def _is_ignored_path(self, ignored_dir_re, f):
        # Remove upto topsrcdir in pathname and match
        if f.startswith(self.topsrcdir + '/'):
            match_f = f[len(self.topsrcdir + '/'):]
        else:
            match_f = f
        return re.match(ignored_dir_re, match_f)

    def _generate_path_list(self, paths, verbose=True):
        path_to_third_party = os.path.join(self.topsrcdir, self._format_ignore_file)
        ignored_dir = []
        with open(path_to_third_party, 'r') as fh:
            for line in fh:
                # Remove comments and empty lines
                if line.startswith('#') or len(line.strip()) == 0:
                    continue
                # The regexp is to make sure we are managing relative paths
                ignored_dir.append(r"^[\./]*" + line.rstrip())

        # Generates the list of regexp
        ignored_dir_re = '(%s)' % '|'.join(ignored_dir)
        extensions = self._format_include_extensions

        path_list = []
        for f in paths:
            if self._is_ignored_path(ignored_dir_re, f):
                # Early exit if we have provided an ignored directory
                if verbose:
                    print("clang-format: Ignored third party code '{0}'".format(f))
                continue

            if os.path.isdir(f):
                # Processing a directory, generate the file list
                for folder, subs, files in os.walk(f):
                    subs.sort()
                    for filename in sorted(files):
                        f_in_dir = os.path.join(folder, filename)
                        if (f_in_dir.endswith(extensions)
                            and not self._is_ignored_path(ignored_dir_re, f_in_dir)):
                            # Supported extension and accepted path
                            path_list.append(f_in_dir)
            else:
                if f.endswith(extensions):
                    path_list.append(f)

        return path_list

    def _run_clang_format_in_console(self, clang_format, paths, assume_filename):
        path_list = self._generate_path_list(assume_filename, False)

        if path_list == []:
            return 0

        # We use -assume-filename in order to better determine the path for
        # the .clang-format when it is ran outside of the repo, for example
        # by the extension hg-formatsource
        args = [clang_format, "-assume-filename={}".format(assume_filename[0])]

        process = subprocess.Popen(args, stdin=subprocess.PIPE)
        with open(paths[0], 'r') as fin:
            process.stdin.write(fin.read())
            process.stdin.close()
            process.wait();
            return 0

    def _run_clang_format_path(self, clang_format, show, paths):
        # Run clang-format on files or directories directly
        from subprocess import check_output, CalledProcessError

        args = [clang_format, "-i"]

        path_list = self._generate_path_list(paths)

        if path_list == []:
            return

        print("Processing %d file(s)..." % len(path_list))

        batchsize = 200

        for i in range(0, len(path_list), batchsize):
            l = path_list[i: (i + batchsize)]
            # Run clang-format on the list
            try:
                check_output(args + l)
            except CalledProcessError as e:
                # Something wrong happend
                print("clang-format: An error occured while running clang-format.")
                return e.returncode

        if show:
            # show the diff
            if self.repository.name == 'hg':
                diff_command = ["hg", "diff"] + paths
            else:
                assert self.repository.name == 'git'
                diff_command = ["git", "diff"] + paths
            try:
                output = check_output(diff_command)
                print(output)
            except CalledProcessError as e:
                # Something wrong happend
                print("clang-format: Unable to run the diff command.")
                return e.returncode
        return 0

@CommandProvider
class Vendor(MachCommandBase):
    """Vendor third-party dependencies into the source repository."""

    @Command('vendor', category='misc',
             description='Vendor third-party dependencies into the source repository.')
    def vendor(self):
        self.parser.print_usage()
        sys.exit(1)

    @SubCommand('vendor', 'rust',
                description='Vendor rust crates from crates.io into third_party/rust')
    @CommandArgument('--ignore-modified', action='store_true',
        help='Ignore modified files in current checkout',
        default=False)
    @CommandArgument('--build-peers-said-large-imports-were-ok', action='store_true',
        help='Permit overly-large files to be added to the repository',
        default=False)
    def vendor_rust(self, **kwargs):
        from mozbuild.vendor_rust import VendorRust
        vendor_command = self._spawn(VendorRust)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'aom',
                description='Vendor av1 video codec reference implementation into the source repository.')
    @CommandArgument('-r', '--revision',
        help='Repository tag or commit to update to.')
    @CommandArgument('--repo',
        help='Repository url to pull a snapshot from. Supports github and googlesource.')
    @CommandArgument('--ignore-modified', action='store_true',
        help='Ignore modified files in current checkout',
        default=False)
    def vendor_aom(self, **kwargs):
        from mozbuild.vendor_aom import VendorAOM
        vendor_command = self._spawn(VendorAOM)
        vendor_command.vendor(**kwargs)
    @SubCommand('vendor', 'dav1d',
                description='Vendor dav1d implementation of AV1 into the source repository.')
    @CommandArgument('-r', '--revision',
        help='Repository tag or commit to update to.')
    @CommandArgument('--repo',
        help='Repository url to pull a snapshot from. Supports gitlab.')
    @CommandArgument('--ignore-modified', action='store_true',
        help='Ignore modified files in current checkout',
        default=False)
    def vendor_dav1d(self, **kwargs):
        from mozbuild.vendor_dav1d import VendorDav1d
        vendor_command = self._spawn(VendorDav1d)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'python',
                description='Vendor Python packages from pypi.org into third_party/python')
    @CommandArgument('--with-windows-wheel', action='store_true',
        help='Vendor a wheel for Windows along with the source package',
        default=False)
    @CommandArgument('packages', default=None, nargs='*', help='Packages to vendor. If omitted, packages and their dependencies defined in Pipfile.lock will be vendored. If Pipfile has been modified, then Pipfile.lock will be regenerated. Note that transient dependencies may be updated when running this command.')
    def vendor_python(self, **kwargs):
        from mozbuild.vendor_python import VendorPython
        vendor_command = self._spawn(VendorPython)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'manifest',
                description='Vendor externally hosted repositories into this '
                            'repository.')
    @CommandArgument('files', nargs='+',
                     help='Manifest files to work on')
    @CommandArgumentGroup('verify')
    @CommandArgument('--verify', '-v', action='store_true', group='verify',
                     required=True, help='Verify manifest')
    def vendor_manifest(self, files, verify):
        from mozbuild.vendor_manifest import verify_manifests
        verify_manifests(files)

@CommandProvider
class WebRTCGTestCommands(GTestCommands):
    @Command('webrtc-gtest', category='testing',
        description='Run WebRTC.org GTest unit tests.')
    @CommandArgument('gtest_filter', default=b"*", nargs='?', metavar='gtest_filter',
        help="test_filter is a ':'-separated list of wildcard patterns (called the positive patterns),"
             "optionally followed by a '-' and another ':'-separated pattern list (called the negative patterns).")
    @CommandArgumentGroup('debugging')
    @CommandArgument('--debug', action='store_true', group='debugging',
        help='Enable the debugger. Not specifying a --debugger option will result in the default debugger being used.')
    @CommandArgument('--debugger', default=None, type=str, group='debugging',
        help='Name of debugger to use.')
    @CommandArgument('--debugger-args', default=None, metavar='params', type=str,
        group='debugging',
        help='Command-line arguments to pass to the debugger itself; split as the Bourne shell would.')
    def gtest(self, gtest_filter, debug, debugger,
              debugger_args):
        app_path = self.get_binary_path('webrtc-gtest')
        args = [app_path]

        if debug or debugger or debugger_args:
            args = self.prepend_debugger_args(args, debugger, debugger_args)

        # Used to locate resources used by tests
        cwd = os.path.join(self.topsrcdir, 'media', 'webrtc', 'trunk')

        if not os.path.isdir(cwd):
            print('Unable to find working directory for tests: %s' % cwd)
            return 1

        gtest_env = {
            # These tests are not run under ASAN upstream, so we need to
            # disable some checks.
            b'ASAN_OPTIONS': 'alloc_dealloc_mismatch=0',
            # Use GTest environment variable to control test execution
            # For details see:
            # https://code.google.com/p/googletest/wiki/AdvancedGuide#Running_Test_Programs:_Advanced_Options
            b'GTEST_FILTER': gtest_filter
        }

        return self.run_process(args=args,
                                append_env=gtest_env,
                                cwd=cwd,
                                ensure_exit_code=False,
                                pass_thru=True)

@CommandProvider
class Repackage(MachCommandBase):
    '''Repackages artifacts into different formats.

    This is generally used after packages are signed by the signing
    scriptworkers in order to bundle things up into shippable formats, such as a
    .dmg on OSX or an installer exe on Windows.
    '''
    @Command('repackage', category='misc',
             description='Repackage artifacts into different formats.')
    def repackage(self):
        print("Usage: ./mach repackage [dmg|installer|mar] [args...]")

    @SubCommand('repackage', 'dmg',
                description='Repackage a tar file into a .dmg for OSX')
    @CommandArgument('--input', '-i', type=str, required=True,
        help='Input filename')
    @CommandArgument('--output', '-o', type=str, required=True,
        help='Output filename')
    def repackage_dmg(self, input, output):
        if not os.path.exists(input):
            print('Input file does not exist: %s' % input)
            return 1

        if not os.path.exists(os.path.join(self.topobjdir, 'config.status')):
            print('config.status not found.  Please run |mach configure| '
                  'prior to |mach repackage|.')
            return 1

        from mozbuild.repackaging.dmg import repackage_dmg
        repackage_dmg(input, output)

    @SubCommand('repackage', 'installer',
                description='Repackage into a Windows installer exe')
    @CommandArgument('--tag', type=str, required=True,
        help='The .tag file used to build the installer')
    @CommandArgument('--setupexe', type=str, required=True,
        help='setup.exe file inside the installer')
    @CommandArgument('--package', type=str, required=False,
        help='Optional package .zip for building a full installer')
    @CommandArgument('--output', '-o', type=str, required=True,
        help='Output filename')
    @CommandArgument('--package-name', type=str, required=False,
        help='Name of the package being rebuilt')
    @CommandArgument('--sfx-stub', type=str, required=True,
        help='Path to the self-extraction stub.')
    def repackage_installer(self, tag, setupexe, package, output, package_name, sfx_stub):
        from mozbuild.repackaging.installer import repackage_installer
        repackage_installer(
            topsrcdir=self.topsrcdir,
            tag=tag,
            setupexe=setupexe,
            package=package,
            output=output,
            package_name=package_name,
            sfx_stub=sfx_stub,
        )

    @SubCommand('repackage', 'msi',
                description='Repackage into a MSI')
    @CommandArgument('--wsx', type=str, required=True,
        help='The wsx file used to build the installer')
    @CommandArgument('--version', type=str, required=True,
        help='The Firefox version used to create the installer')
    @CommandArgument('--locale', type=str, required=True,
        help='The locale of the installer')
    @CommandArgument('--arch', type=str, required=True,
        help='The archtecture you are building x64 or x32')
    @CommandArgument('--setupexe', type=str, required=True,
        help='setup.exe installer')
    @CommandArgument('--candle', type=str, required=False,
        help='location of candle binary')
    @CommandArgument('--light', type=str, required=False,
        help='location of light binary')
    @CommandArgument('--output', '-o', type=str, required=True,
        help='Output filename')
    def repackage_msi(self, wsx, version, locale, arch, setupexe, candle, light, output):
        from mozbuild.repackaging.msi import repackage_msi
        repackage_msi(
            topsrcdir=self.topsrcdir,
            wsx=wsx,
            version=version,
            locale=locale,
            arch=arch,
            setupexe=setupexe,
            candle=candle,
            light=light,
            output=output,
        )

    @SubCommand('repackage', 'mar',
                description='Repackage into complete MAR file')
    @CommandArgument('--input', '-i', type=str, required=True,
        help='Input filename')
    @CommandArgument('--mar', type=str, required=True,
        help='Mar binary path')
    @CommandArgument('--output', '-o', type=str, required=True,
        help='Output filename')
    @CommandArgument('--format', type=str, default='lzma',
        choices=('lzma', 'bz2'),
        help='Mar format')
    def repackage_mar(self, input, mar, output, format):
        from mozbuild.repackaging.mar import repackage_mar
        repackage_mar(self.topsrcdir, input, mar, output, format)

@CommandProvider
class Analyze(MachCommandBase):
    """ Get information about a file in the build graph """
    @Command('analyze', category='misc',
        description='Analyze the build graph.')
    def analyze(self):
        print("Usage: ./mach analyze [files|report] [args...]")

    @SubCommand('analyze', 'files',
        description='Get incremental build cost for file(s) from the tup database.')
    @CommandArgument('--path', help='Path to tup db',
        default=None)
    @CommandArgument('files', nargs='*', help='Files to analyze')
    def analyze_files(self, path, files):
        from mozbuild.analyze.graph import Graph
        if path is None:
            path = mozpath.join(self.topsrcdir, '.tup', 'db')
        if os.path.isfile(path):
            g = Graph(path)
            g.file_summaries(files)
            g.close()
        else:
            res = 'Please make sure you have a local tup db *or* specify the location with --path.'
            print ('Could not find a valid tup db in ' + path, res, sep='\n')
            return 1

    @SubCommand('analyze', 'all',
        description='Get a report of files changed within the last n days and their corresponding build cost.')
    @CommandArgument('--days', '-d', type=int, default=14,
        help='Number of days to include in the report.')
    @CommandArgument('--format', default='pretty',
        choices=['pretty', 'csv', 'json', 'html'],
        help='Print or export data in the given format.')
    @CommandArgument('--limit', type=int, default=None,
        help='Get the top n most expensive files from the report.')
    @CommandArgument('--path', help='Path to cost_dict.gz',
        default=None)
    def analyze_report(self, days, format, limit, path):
        from mozbuild.analyze.hg import Report
        self._activate_virtualenv()
        try:
            self.virtualenv_manager.install_pip_package('tablib==0.12.1')
        except Exception:
            print ('Could not install tablib via pip.')
            return 1
        if path is None:
            # go find tup db and make a cost_dict
            from mozbuild.analyze.graph import Graph
            db_path = mozpath.join(self.topsrcdir, '.tup', 'db')
            if os.path.isfile(db_path):
                g = Graph(db_path)
                r = Report(days, cost_dict=g.get_cost_dict())
                g.close()
                r.generate_output(format, limit, self.topobjdir)
            else:
                res = 'Please specify the location of cost_dict.gz with --path.'
                print ('Could not find %s to make a cost dictionary.' % db_path, res, sep='\n')
                return 1
        else:
            # path to cost_dict.gz was specified
            if os.path.isfile(path):
                r = Report(days, path)
                r.generate_output(format, limit, self.topobjdir)
            else:
                res = 'Please specify the location of cost_dict.gz with --path.'
                print ('Could not find cost_dict.gz at %s' % path, res, sep='\n')
                return 1


@SettingsProvider
class TelemetrySettings():
    config_settings = [
        ('build.telemetry', 'boolean', """
Enable submission of build system telemetry.
        """.strip(), False),
    ]
