/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export async function runBackgroundTask(_commandLine) {
  // Disable logging of Console to stdout in order to focus on MOZ_LOG ones
  Services.prefs.setBoolPref("devtools.console.stdout.chrome", false);

  // Log the main thread PID so that the test can identify it easily
  const pid = Services.appinfo.processID;
  dump(`CONSOLE-PID:${pid}\n`);

  console.log("foo");
  console.debug("bar");
  const prefixed = console.createInstance({ prefix: "my-prefix" });
  prefixed.error({
    shouldLogError: prefixed.shouldLog("Error"),
    shouldLogLog: prefixed.shouldLog("Log"),
  });
  prefixed.warn("warning");
  prefixed.log("not-logged");

  return 0;
}
