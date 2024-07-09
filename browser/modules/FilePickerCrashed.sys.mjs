/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  DownloadsCommon: "resource:///modules/DownloadsCommon.sys.mjs",
});

const L10MessageSelectorMap = {
  "crashed:open": "file-picker-crashed-open",
  "crashed:save-somewhere": "file-picker-crashed-save-somewhere",
  "crashed:save-nowhere": "file-picker-crashed-save-nowhere",
  "failed:open": "file-picker-failed-open",
  "failed:save-somewhere": "file-picker-failed-save-somewhere",
  "failed:save-nowhere": "file-picker-failed-save-nowhere",
};

export const FilePickerCrashed = {
  async observe(subject, topic, _data) {
    const bag = subject.QueryInterface(Ci.nsIPropertyBag2);
    const ctx = bag.getPropertyAsInterface("ctx", Ci.nsILoadContext);

    const nbox = (() => {
      let window = ctx.topChromeWindow;

      // If our associated window isn't a browser window (e.g., it's an
      // extension window or the History window or the like), just grab the
      // topmost browser window.
      if (!window?.gBrowser) {
        window = lazy.BrowserWindowTracker.getTopWindow();
      }

      // If there _is_ no topmost browser window... throw an error and hope it
      // shows up in logs somewhere?
      if (!window) {
        const err = new Error(
          "file picker crashed, but no browser windows were available to report this"
        );
        // (for further investigation via the browser console, if that's accessible)
        console.error({ err, bag, ctx });
        throw err;
      }

      // This will get the notification-box for the window's currently-shown
      // tab, which may or may not be the tab which attempted to spawn a
      // file-dialog. This only really matters for delayed-open but instafailing
      // save-dialogs; hopefully the filename will presumably be sufficient to
      // disambiguate.
      return window.gBrowser.getNotificationBox();
    })();

    const mode = bag.getPropertyAsUint32("mode");

    const isCrash = bag.getPropertyAsBool("crash");

    const isOpen = mode != Ci.nsIFilePicker.modeSave;
    const file = (() => {
      if (isOpen) {
        return null;
      }

      try {
        return bag.getPropertyAsInterface("file", Ci.nsIFile);
      } catch (e) {
        // property presumably not present; proceed
      }

      try {
        // This probably isn't user-actionable, but may be useful to developers
        const file_error = bag.getPropertyAsUint32("file-error");
        console.error(
          "Failed to get fallback file location: nsresult 0x" +
            file_error.toString(16).padLeft(8, 0)
        );
      } catch (e) {
        // Report this meta-error to the browser console; then continue onward to
        // also report the original failure.
        console.error(e);
      }

      return null;
    })();

    const cause = isCrash ? "crashed" : "failed";
    const consequence = isOpen
      ? "open"
      : "save-" + (file ? "somewhere" : "nowhere");
    const msgId = L10MessageSelectorMap[cause + ":" + consequence];

    const label = {
      "l10n-id": msgId,
      "l10n-args": file ? { path: file.path } : {},
    };

    const buttons = [];

    /* TODO(rkraesig): add "More Info" button? */

    // Offer to show the file's location, if one is provided.
    if (file) {
      buttons.push({
        "l10n-id": "file-picker-crashed-show-in-folder",
        callback() {
          lazy.DownloadsCommon.showDownloadedFile(file);
        },
      });
    }

    const notification = await nbox.appendNotification(
      topic,
      {
        label,
        image: "chrome://global/skin/icons/error.svg",
        priority: nbox.PRIORITY_CRITICAL_LOW,
      },
      buttons
    );
    // Persist the notification until the user removes it.
    notification.persistence = -1;
  },
};
