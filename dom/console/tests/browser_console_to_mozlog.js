/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* Use console API to log via MOZ_LOG to stdout/file/profiler */

// Use background task in order to control MOZ_LOG env variable passed to another gecko run
const { BackgroundTasksTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/BackgroundTasksTestUtils.sys.mjs"
);
BackgroundTasksTestUtils.init(this);
const do_backgroundtask = BackgroundTasksTestUtils.do_backgroundtask.bind(
  BackgroundTasksTestUtils
);

add_task(async function test_console_to_mozlog() {
  const lines = [];
  const promise = do_backgroundtask("console", {
    onStdoutLine: (line, _proc) => {
      dump(`STDOUT: ${line}`);
      lines.push(line);
    },
    extraEnv: {
      MOZ_LOG: "console:5,my-prefix:2",
    },
  });
  const exitCode = await promise;
  is(exitCode, 0);

  const pidLine = lines.find(line => line.includes("CONSOLE-PID"));
  ok(pidLine, "Found the line where the parent process PID is logged");
  const [, pid] = pidLine.split(":");
  ok(pid, "Got the pid out of the PID line");

  // Each MOZ_LOG / console api call starts with a description of the process and thread where it is logged
  const threadPrefix = `[Parent ${pid}: Main Thread]: `;

  const expectedLogs = [
    `I/console log: "foo"`,
    `D/console debug: "bar"`,
    // Bug 1923985: For now, the console API level isn't synchronized with MOZ_LOG one.
    // shouldLogLog should be false because of my-prefix set to level 2.
    `E/my-prefix error: ({shouldLogError:true, shouldLogLog:true})`,
    `W/my-prefix warn: "warning"`,
  ];

  for (const expected of expectedLogs) {
    ok(
      lines.some(line => line.includes(`${threadPrefix}${expected}`)),
      `Found ${expected}`
    );
  }

  // The console.log call with my-prefix isn't logged because of log level set to "2" for my-prefix
  ok(
    !lines.some(line => line.includes("not-logged")),
    "Logs blocked by too verbose level aren't visible in stdout"
  );
});
