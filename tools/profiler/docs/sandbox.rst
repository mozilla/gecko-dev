Profiling Sandbox violations (Linux)
====================================

Recoding sandbox violations
---------------------------

The profiler now offers a way to track sandbox violations happening from child
processes on Linux systems. One can also rely on `MOZ_PROFILER_STARTUP=1`.
Please make sure you select the `Debug` preset.

It will record sandbox requests (child process system calls intercepted) as
well as audit (deny decision, whether the sandbox is running in permissive mode
or not). This should also record `SANDBOX_LOG` statements, including the policy
if the process is started when the profiler is running.

We manage to capture the call stack on the child process and pass that to the
profiler, so hopefully any thread in our child process will report a stack
explaining why the syscall was made.

Capturing the stack might require either a nightly build (opt or debug), or a
beta/release build with debug enabled.

We report markers on the `SandboxProfilerEmitterSyscalls` thread for syscalls
and `SandboxProfilerEmitterLogs` for `SANDBOX_LOG` entries.

Analyzing data
--------------

The sandbox on Linux works by intercepting child processes system calls, and
via a communication channel to the parent process, decide whether we allow or
not, and maybe perform brokering.

Because we generate data on the child and on the parent process, there is a
pairing system in place: each child process is going to wrap an identifier (an
int) within its sandbox requests, that will be visible on the markers table of
that child. Parent process will have an `FSBrokerXXX` thread for each child
process (where `XXX` is the PID of the child), and attached markers for
permissive or denial audit.

One should select one or all of the `SandboxProfilerEmitterSyscalls` or
`SandboxProfilerEmitterLogs` thread(s) on child process(es), and matching
`FSBrokerXXX` thread(s) on the parent process. Then it is just a matter, within
the correct pair of child/parent threads, to match requests IDs with audit IDs
to uncover the valuable information.

Those would include, on the child side:
 - PID;
 - syscall name;
 - syscall flags;
 - path parameters when some;

And on the parent side in case of denial,
 - Child PID;
 - syscall name;
 - syscall flags;
 - permissions;
