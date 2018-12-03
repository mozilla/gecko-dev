/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  Services: "resource://gre/modules/Services.jsm",
  SessionStartup: "resource:///modules/sessionstore/SessionStartup.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  StartupPerformance: "resource:///modules/sessionstore/StartupPerformance.jsm",
});

/* globals ExtensionAPI */

this.sessionrestore = class extends ExtensionAPI {
  onStartup() {
    // run() is async but we don't want to await or return it here,
    // since the extension should be considered started even before
    // run() has finished all its work.
    this.run();
  }

  async ensureTalosParentProfiler() {
    // TalosParentProfiler is part of TalosPowers, which is a separate WebExtension
    // that may or may not already have finished loading at this point. getTalosParentProfiler
    // is used to wait for TalosPowers to be around before continuing. This should
    // not be used to delay the execution of the test, but to delay the dumping of
    // the profile to disk.
    async function getTalosParentProfiler() {
      try {
        ChromeUtils.import("resource://talos-powers/TalosParentProfiler.jsm");
        return TalosParentProfiler;
      } catch (err) {
        await new Promise(resolve => setTimeout(resolve, 500));
        return getTalosParentProfiler();
      }
    }

    this.TalosParentProfiler = await getTalosParentProfiler();
  }

  async finishProfiling(msg) {
    await this.ensureTalosParentProfiler();

    let win = BrowserWindowTracker.getTopWindow();
    let args = win.arguments[0];
    if (args && args instanceof Ci.nsIArray) {
      // For start-up tests Gecko Profiler arguments are passed to the first URL in
      // the query string, with the presumption that some tab that is loaded at start-up
      // will want to use them in the TalosContentProfiler.js script.
      //
      // Because we're running this part of the test in the parent process, we
      // pull those arguments from the top window directly. This is mainly so
      // that TalosParentProfiler knows where to save the profile.
      Cu.importGlobalProperties(["URL"]);
      let url = new URL(args.queryElementAt(0, Ci.nsISupportsString).data);
      this.TalosParentProfiler.initFromURLQueryParams(url.search);
    }

    await this.TalosParentProfiler.pause(msg);
    await this.TalosParentProfiler.finishStartupProfiling();
  }

  async run() {
    try {
      let didRestore = true;

      if (!StartupPerformance.isRestored) {
        await SessionStartup.onceInitialized;

        if (SessionStartup.sessionType == SessionStartup.NO_SESSION ||
            SessionStartup.sessionType == SessionStartup.DEFER_SESSION) {
          // We should only hit this patch in sessionrestore_no_auto_restore
          if (!Services.prefs.getBoolPref("talos.sessionrestore.norestore", false)) {
            throw new Error("Session was not restored!");
          }
          await this.finishProfiling("This test measures the time between sessionRestoreInit " +
                                     "and sessionRestored, ignore everything around that");

          didRestore = false;
        } else {
          await new Promise(resolve => {
            let observe = async () => {
              Services.obs.removeObserver(observe, StartupPerformance.RESTORED_TOPIC);
              await this.finishProfiling("This test measures the time between sessionRestoreInit " +
                                         "and the last restored tab, ignore everything around that");

              resolve();
            };
            Services.obs.addObserver(observe, StartupPerformance.RESTORED_TOPIC);
          });
        }
      }

      let startup_info = Services.startup.getStartupInfo();
      let restoreTime = didRestore
                      ? StartupPerformance.latestRestoredTimeStamp
                      : startup_info.sessionRestored;
      let duration = restoreTime - startup_info.sessionRestoreInit;

      // Report data to Talos, if possible.
      dump("__start_report" + duration + "__end_report\n\n");

      // Next one is required by the test harness but not used.
      // eslint-disable-next-line mozilla/avoid-Date-timing
      dump("__startTimestamp" + Date.now() + "__endTimestamp\n\n");
    } catch (ex) {
      dump(`SessionRestoreTalosTest: error ${ex}\n`);
      dump(ex.stack);
      dump("\n");
    }

    Services.startup.quit(Services.startup.eForceQuit);
  }
};
