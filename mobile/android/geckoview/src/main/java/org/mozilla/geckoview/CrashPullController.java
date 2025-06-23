/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * The CrashPullController offers a way to get notified of JS level Crash Pull events, that may be
 * triggered by incoming Remote Settings requests offering developpers a way to collect specific
 * crash reports that may be required for investigation on specific bugs.
 *
 * <p>Upon the event triggered by the JS level code in GeckoViewStartup and assuming a
 * CrashPullController.Delegate was registered, then its onCrashPull gets called to perform the rest
 * of the handling, i.e., notifying the user to ask for those crash submission.
 */
public class CrashPullController {
  private static final String LOGTAG = "CrashPull";

  /**
   * Implement this interface to get notification of a remote settings request to submit one or more
   * crash reports.
   */
  public interface Delegate {
    /**
     * Handling of the crash pull delegation: creation of a CrashAction that will end up showing the
     * user a dialog to explain specific crash reports are of use and query for their submission.
     *
     * @param crashIDs The list of crash IDs directly pointing to the crash dump file with the full
     *     path
     */
    @UiThread
    default void onCrashPull(@NonNull final String[] crashIDs) {}
  }

  /* package */ static final class CrashPullProxy implements BundleEventListener {
    private static final String CRASH_PULL_EVENT = "GeckoView:RemoteSettingsCrashPull";

    public CrashPullProxy() {}

    private @Nullable Delegate mDelegate;

    public synchronized void setDelegate(final @Nullable Delegate delegate) {
      if (mDelegate == delegate) {
        return;
      }
      if (mDelegate != null) {
        unregisterListener();
      }

      mDelegate = delegate;

      if (mDelegate != null) {
        registerListener();
      }
    }

    public synchronized @Nullable Delegate getDelegate() {
      return mDelegate;
    }

    private void registerListener() {
      Log.d(LOGTAG, "registerListener");
      EventDispatcher.getInstance()
          .dispatch("GeckoView:CrashPullController.Delegate:Attached", null);
      EventDispatcher.getInstance().registerUiThreadListener(this, CRASH_PULL_EVENT);
    }

    private void unregisterListener() {
      Log.d(LOGTAG, "unregisterListener");
      EventDispatcher.getInstance().unregisterUiThreadListener(this, CRASH_PULL_EVENT);
    }

    @Override // BundleEventListener
    public synchronized void handleMessage(
        final String event, final GeckoBundle message, final EventCallback callback) {
      Log.d(LOGTAG, "handleMessage " + event);

      if (mDelegate == null) {
        if (callback != null) {
          callback.sendError("No delegate attached");
        }
        return;
      }

      if (CRASH_PULL_EVENT.equals(event)) {
        final String[] crashIDs = message.getStringArray("crashIDs");
        Log.d(LOGTAG, "Adding crashIDs: " + crashIDs);
        if (crashIDs != null) {
          mDelegate.onCrashPull(crashIDs);
        }
      }
    }
  }
}
