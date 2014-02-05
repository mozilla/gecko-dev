#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import traceback

from mozprocess.processhandler import ProcessHandler
import mozcrash
import mozlog

# we can replace this method with 'abc'
# (http://docs.python.org/library/abc.html) when we require Python 2.6+
def abstractmethod(method):
  line = method.func_code.co_firstlineno
  filename = method.func_code.co_filename
  def not_implemented(*args, **kwargs):
    raise NotImplementedError('Abstract method %s at File "%s", line %s '
                              'should be implemented by a concrete class' %
                              (repr(method), filename, line))
  return not_implemented

class RunnerNotStartedError(Exception):
    """Exception handler in case the runner is not started."""

class Runner(object):

    def __init__(self, profile, clean_profile=True, process_class=None,
                 kp_kwargs=None, env=None, symbols_path=None):
        self.clean_profile = clean_profile
        self.env = env or {}
        self.kp_kwargs = kp_kwargs or {}
        self.process_class = process_class or ProcessHandler
        self.process_handler = None
        self.returncode = None
        self.profile = profile
        self.log = mozlog.getLogger('MozRunner')
        self.symbols_path = symbols_path

    def start(self, *args, **kwargs):
        """
        Run the process
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

    def wait(self, timeout=None):
        """
        Wait for the process to exit.
        Returns the process return code if the process exited,
        returns -<signal> if the process was killed (Unix only)
        returns None if the process is still running.

        :param timeout: if not None, will return after timeout seconds.
                        Use is_running() to determine whether or not a
                        timeout occured. Timeout is ignored if
                        interactive was set to True.
        """
        if self.process_handler is not None:
            if isinstance(self.process_handler, subprocess.Popen):
                self.returncode = self.process_handler.wait()
            else:
                self.process_handler.wait(timeout)

                if not self.process_handler:
                    # the process was killed by another thread
                    return self.returncode

                # the process terminated, retrieve the return code
                self.returncode = self.process_handler.proc.poll()
                if self.returncode is not None:
                    self.process_handler = None
        elif self.returncode is None:
            raise RunnerNotStartedError("Wait called before runner started")

        return self.returncode

    def is_running(self):
        """
        Returns True if the process is still running, False otherwise
        """
        return self.process_handler is not None


    def stop(self, sig=None):
        """
        Kill the process

        :param sig: Signal used to kill the process, defaults to SIGKILL
                    (has no effect on Windows).
        """
        if self.process_handler is None:
            return
        self.returncode = self.process_handler.kill(sig=sig)
        self.process_handler = None

    def reset(self):
        """
        Reset the runner to its default state
        """
        if getattr(self, 'profile', False):
            self.profile.reset()

    def check_for_crashes(self, dump_directory=None, test_name=None):
        if not dump_directory:
            dump_directory = os.path.join(self.profile.profile, 'minidumps')

        crashed = False
        try:
            crashed = mozcrash.check_for_crashes(dump_directory,
                                                 self.symbols_path,
                                                 test_name=test_name)
        except:
            traceback.print_exc()
        return crashed

    def cleanup(self):
        """
        Cleanup all runner state
        """
        if self.is_running():
            self.stop()
        if getattr(self, 'profile', False) and self.clean_profile:
            self.profile.cleanup()

    __del__ = cleanup
