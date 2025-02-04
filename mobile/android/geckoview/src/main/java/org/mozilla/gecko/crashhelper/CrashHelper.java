/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.crashhelper;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.geckoview.BuildConfig;

public final class CrashHelper extends Service {
  private static final String LOGTAG = "GeckoCrashHelper";
  private static final boolean DEBUG = !BuildConfig.MOZILLA_OFFICIAL;
  private static boolean sNativeLibLoaded;

  @Override
  public synchronized void onCreate() {
    Log.d(LOGTAG, "CrashHelper.onCreate()");
    if (!sNativeLibLoaded) {
      if (DEBUG) {
        Log.d(LOGTAG, "Loading crash helper library");
      }
      GeckoLoader.doLoadLibrary(this, "crashhelper");
      if (DEBUG) {
        Log.d(LOGTAG, "Crash helper library loaded");
      }
      sNativeLibLoaded = true;
    }
  }

  @Override
  public int onStartCommand(final Intent intent, final int flags, final int startId) {
    Log.d(LOGTAG, "CrashHelper.onStartCommand()");
    if (intent == null) {
      Log.d(LOGTAG, "Intent is empty, crash helper will not start");
      return Service.START_NOT_STICKY;
    }

    final int clientPid = intent.getIntExtra("ClientPid", 0);

    if (clientPid == 0) {
      Log.d(LOGTAG, "Missing ClientPid integer, crash helper will not start");
      return Service.START_NOT_STICKY;
    }

    crash_generator(clientPid);

    return Service.START_STICKY;
  }

  @Override
  public IBinder onBind(final Intent intent) {
    Log.d(LOGTAG, "CrashHelper.onBind()");

    // We don't provide a binding, so return null
    return null;
  }

  // Note: we don't implement onDestroy() because the service has the
  // `stopWithTask` flag set in the manifest, so the service manager will
  // tear it down for us.

  private static native void crash_generator(int clientPid);
}
