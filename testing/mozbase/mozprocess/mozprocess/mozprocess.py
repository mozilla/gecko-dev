# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
import threading
import time
from queue import Empty, Queue


def run_and_wait(
    args=None,
    cwd=None,
    env=None,
    text=True,
    timeout=None,
    timeout_handler=None,
    output_timeout=None,
    output_timeout_handler=None,
    output_line_handler=None,
):
    """
    Run a process and wait for it to complete, with optional support for
    line-by-line output handling and timeouts.

    On timeout or output timeout, the callback should kill the process;
    many clients use  mozcrash.kill_and_get_minidump() in the timeout
    callback.

    run_and_wait is not intended to be a generic replacement for subprocess.
    Clients requiring different options or behavior should use subprocess
    directly.

    :param args: command to run. May be a string or a list.
    :param cwd: working directory for command.
    :param env: environment to use for the process (defaults to os.environ).
    :param text: open streams in text mode if True; else use binary mode.
    :param timeout: seconds to wait for process to complete before calling timeout_handler
    :param timeout_handler: function to be called if timeout reached
    :param output_timeout: seconds to wait for process to generate output
    :param output_timeout_handler: function to be called if output_timeout is reached
    :param output_line_handler: function to be called for every line of process output
    """
    is_win = os.name == "nt"

    if env is None:
        env = os.environ.copy()

    if is_win:
        kwargs = {
            "creationflags": subprocess.CREATE_NEW_PROCESS_GROUP,
        }
    else:
        kwargs = {
            "preexec_fn": os.setsid,
        }

    start_time = time.time()
    if timeout is not None:
        timeout += start_time

    queue = Queue()

    def get_line():
        queue_timeout = None
        if timeout:
            queue_timeout = timeout - time.time()
        if output_timeout:
            if queue_timeout:
                queue_timeout = min(queue_timeout, output_timeout)
            else:
                queue_timeout = output_timeout
        return queue.get(timeout=queue_timeout)

    proc = subprocess.Popen(
        args,
        cwd=cwd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=text,
        **kwargs
    )

    def reader(fh, queue):
        for line in iter(fh.readline, "" if text else b""):
            queue.put(line)
        # Give a chance to the reading loop to exit without a timeout.
        queue.put(b"")

    threading.Thread(
        name="reader",
        target=reader,
        args=(
            proc.stdout,
            queue,
        ),
        daemon=True,
    ).start()

    try:
        for line in iter(get_line, b""):
            if output_line_handler:
                output_line_handler(proc, line)
            else:
                print(line)
        proc.wait()
    except Empty:
        if timeout and time.time() < timeout or not timeout:
            if output_timeout_handler:
                output_timeout_handler(proc)
        elif timeout_handler:
            timeout_handler(proc)

    return proc
