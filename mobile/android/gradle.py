# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import sys
import time
from contextlib import contextmanager

import mozpack.path as mozpath
from filelock import SoftFileLock
from mozbuild.dirutils import ensureParentDir


@contextmanager
def gradle_lock(topobjdir, max_wait_seconds=600):
    # Building the same Gradle root project with multiple concurrent processes
    # is not well supported, so we use a simple lock file to serialize build
    # steps.
    lock_path = "{}/gradle/mach_android.lockfile".format(topobjdir)
    ensureParentDir(lock_path)
    with SoftFileLock(lock_path, timeout=max_wait_seconds):
        yield


def main(dummy_output_file, *args):
    env = dict(os.environ)
    import buildconfig

    cmd = [
        sys.executable,
        mozpath.join(buildconfig.topsrcdir, "mach"),
        "android",
        "export",
    ]
    cmd.extend(args)
    # Confusingly, `MACH` is set only within `mach build`.
    if env.get("MACH"):
        env["GRADLE_INVOKED_WITHIN_MACH_BUILD"] = "1"
    if env.get("LD_LIBRARY_PATH"):
        del env["LD_LIBRARY_PATH"]

    should_print_status = env.get("MACH") and not env.get("NO_BUILDSTATUS_MESSAGES")
    if should_print_status:
        print("BUILDSTATUS " + str(time.time()) + " START_Gradle export")

    subprocess.check_call(cmd, env=env)

    if should_print_status:
        print("BUILDSTATUS " + str(time.time()) + " END_Gradle export")
    return 0
