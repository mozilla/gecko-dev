/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=4 ts=4 sts=4 et
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test_backgroundtask_specific_pref() {
  // First, verify this pref isn't set in Gecko itself.
  Assert.equal(
    -1,
    Services.prefs.getIntPref("test.backgroundtask_specific_pref.exitCode", -1)
  );

  // Second, verify that this pref is set in background tasks.
  // mochitest-chrome tests locally test both unpackaged and packaged
  // builds (with `--appname=dist`).
  let exitCode = await do_backgroundtask("backgroundtask_specific_pref", {
    extraArgs: ["test.backgroundtask_specific_pref.exitCode"],
  });
  Assert.equal(79, exitCode);
});

add_task(async function test_backgroundtask_browser_pref_inherited() {
  // First, verify this pref is set in Firefox.
  Assert.equal(
    15,
    Services.prefs.getIntPref(
      "toolkit.backgroundtasks.tests.browserPrefsInherited",
      -1
    )
  );

  // Second, verify that this pref is set in background tasks.
  let exitCode = await do_backgroundtask("backgroundtask_specific_pref", {
    extraArgs: ["toolkit.backgroundtasks.tests.browserPrefsInherited"],
  });
  Assert.equal(15, exitCode);
});

add_task(async function test_backgroundtask_gecko_pref_overridden() {
  // First, verify this pref is set in Firefox.
  Assert.equal(
    16,
    Services.prefs.getIntPref(
      "toolkit.backgroundtasks.tests.browserPrefsOverriden",
      -1
    )
  );

  // Second, verify that this pref is overridden in background tasks.
  let exitCode = await do_backgroundtask("backgroundtask_specific_pref", {
    extraArgs: ["toolkit.backgroundtasks.tests.browserPrefsOverriden"],
  });
  Assert.equal(26, exitCode);
});

add_task(async function test_backgroundtask_gecko_pref_inherited() {
  // First, verify this pref is set in Gecko.
  Assert.equal(
    17,
    Services.prefs.getIntPref(
      "toolkit.backgroundtasks.tests.geckoPrefsInherited",
      -1
    )
  );

  // Second, verify that this pref is set in background tasks.
  let exitCode = await do_backgroundtask("backgroundtask_specific_pref", {
    extraArgs: ["toolkit.backgroundtasks.tests.geckoPrefsInherited"],
  });
  Assert.equal(17, exitCode);
});

add_task(async function test_backgroundtask_gecko_pref_overridden() {
  // First, verify this pref is set in Gecko.
  Assert.equal(
    18,
    Services.prefs.getIntPref(
      "toolkit.backgroundtasks.tests.geckoPrefsOverriden",
      -1
    )
  );

  // Second, verify that this pref is overridden in background tasks.
  let exitCode = await do_backgroundtask("backgroundtask_specific_pref", {
    extraArgs: ["toolkit.backgroundtasks.tests.geckoPrefsOverriden"],
  });
  Assert.equal(28, exitCode);
});
