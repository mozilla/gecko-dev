/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Handles the download progress indicator in the taskbar.
 */

// Globals

const lazy = {};
const gInterfaces = {};

function defineResettableGetter(object, name, callback) {
  let result = undefined;

  Object.defineProperty(object, name, {
    get() {
      if (typeof result == "undefined") {
        result = callback();
      }

      return result;
    },
    set(value) {
      if (value === null) {
        result = undefined;
      } else {
        throw new Error("don't set this to nonnull");
      }
    },
  });
}

ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  Downloads: "resource://gre/modules/Downloads.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

defineResettableGetter(gInterfaces, "winTaskbar", function () {
  if (!("@mozilla.org/windows-taskbar;1" in Cc)) {
    return null;
  }
  let winTaskbar = Cc["@mozilla.org/windows-taskbar;1"].getService(
    Ci.nsIWinTaskbar
  );
  return winTaskbar.available && winTaskbar;
});

defineResettableGetter(gInterfaces, "macTaskbarProgress", function () {
  return (
    "@mozilla.org/widget/macdocksupport;1" in Cc &&
    Cc["@mozilla.org/widget/macdocksupport;1"].getService(Ci.nsITaskbarProgress)
  );
});

defineResettableGetter(gInterfaces, "gtkTaskbarProgress", function () {
  return (
    "@mozilla.org/widget/taskbarprogress/gtk;1" in Cc &&
    Cc["@mozilla.org/widget/taskbarprogress/gtk;1"].getService(
      Ci.nsIGtkTaskbarProgress
    )
  );
});

/**
 * Handles the download progress indicator in the taskbar.
 */
class DownloadsTaskbarInstance {
  /**
   * Underlying DownloadSummary providing the aggregate download information, or
   * null if the indicator has never been initialized.
   */
  #summary = null;

  /**
   * nsITaskbarProgress object to which download information is dispatched.
   * This can be null if the indicator has never been initialized or if the
   * indicator is currently hidden on Windows.
   */
  #taskbarProgress = null;

  /**
   * The kind of downloads that will be summarized.
   *
   * At registration time, this helps create the DownloadsSummary. When the
   * progress representative unloads, this determines whether the replacement
   * should be a public or a private window.
   */
  #filter = null;

  /**
   * Creates a new DownloadsTaskbarInstance.
   *
   * A given instance of the browser has two instances of this: one for public
   * windows (where aFilter is Downloads.PUBLIC) and the other for private windows
   * (Downloads.PRIVATE).
   *
   * This function doesn't actually register the taskbar with a window; you should
   * call registerIndicator when you add a new window.
   */
  constructor(aFilter) {
    this.#filter = aFilter;
  }

  /**
   * This method is called after a new browser window is opened, and ensures
   * that the download progress indicator is displayed in the taskbar.
   *
   * On Windows, the indicator is attached to the first browser window that
   * calls this method.  When the window is closed, the indicator is moved to
   * another browser window, if available, in no particular order.  When there
   * are no browser windows visible, the indicator is hidden.
   *
   * On Mac OS X, the indicator is initialized globally when this method is
   * called for the first time.  Subsequent calls have no effect.
   *
   * @param aBrowserWindow
   *        nsIDOMWindow object of the newly opened browser window to which the
   *        indicator may be attached.
   */
  async registerIndicator(aBrowserWindow, aForcedBackend) {
    if (!this.#taskbarProgress) {
      if (
        aForcedBackend == "mac" ||
        (!aForcedBackend && gInterfaces.macTaskbarProgress)
      ) {
        // On Mac OS X, we have to register the global indicator only once.
        this.#taskbarProgress = gInterfaces.macTaskbarProgress;
        // Free the XPCOM reference on shutdown, to prevent detecting a leak.
        Services.obs.addObserver(() => {
          this.#taskbarProgress = null;
          gInterfaces.macTaskbarProgress = null;
        }, "quit-application-granted");
      } else if (
        aForcedBackend == "windows" ||
        (!aForcedBackend && gInterfaces.winTaskbar)
      ) {
        // On Windows, the indicator is currently hidden because we have no
        // previous browser window, thus we should attach the indicator now.
        this.#attachIndicator(aBrowserWindow);
      } else if (
        aForcedBackend == "linux" ||
        (!aForcedBackend && gInterfaces.gtkTaskbarProgress)
      ) {
        this.#taskbarProgress = gInterfaces.gtkTaskbarProgress;

        this.#attachGtkTaskbarProgress(aBrowserWindow);
      } else {
        // The taskbar indicator is not available on this platform.
        return;
      }
    }

    // Ensure that the DownloadSummary object will be created asynchronously.
    if (!this.#summary) {
      try {
        let summary = await lazy.Downloads.getSummary(this.#filter);

        if (!this.#summary) {
          this.#summary = summary;
          await this.#summary.addView(this);
        }
      } catch (e) {
        console.error(e);
      }
    }
  }

  /**
   * On Windows, attaches the taskbar indicator to the specified browser window.
   */
  #attachIndicator(aWindow) {
    // Activate the indicator on the specified window.
    let { docShell } = aWindow.browsingContext.topChromeWindow;
    this.#taskbarProgress = gInterfaces.winTaskbar.getTaskbarProgress(docShell);

    // If the DownloadSummary object has already been created, we should update
    // the state of the new indicator, otherwise it will be updated as soon as
    // the DownloadSummary view is registered.
    if (this.#summary) {
      this.onSummaryChanged();
    }

    aWindow.addEventListener("unload", () => {
      // Locate another browser window, excluding the one being closed.
      let browserWindow = this.#determineProgressRepresentative();
      if (browserWindow) {
        // Move the progress indicator to the other browser window.
        this.#attachIndicator(browserWindow);
      } else {
        // The last browser window has been closed.  We remove the reference to
        // the taskbar progress object so that the indicator will be registered
        // again on the next browser window that is opened.
        this.#taskbarProgress = null;
      }
    });
  }

  /**
   * In gtk3, the window itself implements the progress interface.
   */
  #attachGtkTaskbarProgress(aWindow) {
    // Set the current window.
    this.#taskbarProgress.setPrimaryWindow(aWindow);

    // If the DownloadSummary object has already been created, we should update
    // the state of the new indicator, otherwise it will be updated as soon as
    // the DownloadSummary view is registered.
    if (this.#summary) {
      this.onSummaryChanged();
    }

    aWindow.addEventListener("unload", () => {
      // Locate another browser window, excluding the one being closed.
      let browserWindow = this.#determineProgressRepresentative();
      if (browserWindow) {
        // Move the progress indicator to the other browser window.
        this.#attachGtkTaskbarProgress(browserWindow);
      } else {
        // The last browser window has been closed.  We remove the reference to
        // the taskbar progress object so that the indicator will be registered
        // again on the next browser window that is opened.
        this.#taskbarProgress = null;
      }
    });
  }

  /**
   * Determines the next window to represent the downloads' progress.
   */
  #determineProgressRepresentative() {
    if (this.#filter == lazy.Downloads.ALL) {
      return lazy.BrowserWindowTracker.getTopWindow();
    }

    return lazy.BrowserWindowTracker.getTopWindow({
      private: this.#filter == lazy.Downloads.PRIVATE,
    });
  }

  reset() {
    if (this.#summary) {
      this.#summary.removeView(this);
    }

    this.#taskbarProgress = null;
  }

  // DownloadSummary view

  onSummaryChanged() {
    // If the last browser window has been closed, we have no indicator any more.
    if (!this.#taskbarProgress) {
      return;
    }

    if (this.#summary.allHaveStopped || this.#summary.progressTotalBytes == 0) {
      this.#taskbarProgress.setProgressState(
        Ci.nsITaskbarProgress.STATE_NO_PROGRESS,
        0,
        0
      );
    } else if (this.#summary.allUnknownSize) {
      this.#taskbarProgress.setProgressState(
        Ci.nsITaskbarProgress.STATE_INDETERMINATE,
        0,
        0
      );
    } else {
      // For a brief moment before completion, some download components may
      // report more transferred bytes than the total number of bytes.  Thus,
      // ensure that we never break the expectations of the progress indicator.
      let progressCurrentBytes = Math.min(
        this.#summary.progressTotalBytes,
        this.#summary.progressCurrentBytes
      );
      this.#taskbarProgress.setProgressState(
        Ci.nsITaskbarProgress.STATE_NORMAL,
        progressCurrentBytes,
        this.#summary.progressTotalBytes
      );
    }
  }
}

const gDownloadsTaskbarInstances = {};

export var DownloadsTaskbar = {
  async registerIndicator(aWindow, aForcedBackend) {
    let filter = this._selectFilterForWindow(aWindow, aForcedBackend);
    if (!(filter in gDownloadsTaskbarInstances)) {
      gDownloadsTaskbarInstances[filter] = new DownloadsTaskbarInstance(filter);
    }

    await gDownloadsTaskbarInstances[filter].registerIndicator(
      aWindow,
      aForcedBackend
    );
  },

  _selectFilterForWindow(aWindow, aForcedBackend) {
    if (
      aForcedBackend == "windows" ||
      (!aForcedBackend && gInterfaces.winTaskbar)
    ) {
      // On Windows, the private and public windows are separated. Plus, the native code
      // supports multiple taskbar progresses at a time. Therefore, have a separate
      // instance for each.
      return lazy.PrivateBrowsingUtils.isBrowserPrivate(aWindow)
        ? lazy.Downloads.PRIVATE
        : lazy.Downloads.PUBLIC;
    }

    // macOS has a single application icon for all Firefox windows, both private and
    // public. As a result, the Downloads.ALL filter should always be used.
    //
    // On GTK, taskbar progress is indicated by the _NET_WM_XAPP_PROGRESS property for
    // X11, with no Wayland equivalent. Since X11 panels are likely to not group
    // applications, it'd be better to have separate progress bars; however, the native
    // code only supports a single progress bar right now. As such, don't try to have
    // multiple.
    return lazy.Downloads.ALL;
  },

  resetBetweenTests() {
    for (const key of Object.keys(gDownloadsTaskbarInstances)) {
      gDownloadsTaskbarInstances[key].reset();
      delete gDownloadsTaskbarInstances[key];
    }

    gInterfaces.macTaskbarProgress = null;
    gInterfaces.winTaskbar = null;
    gInterfaces.gtkTaskbarProgress = null;
  },
};
