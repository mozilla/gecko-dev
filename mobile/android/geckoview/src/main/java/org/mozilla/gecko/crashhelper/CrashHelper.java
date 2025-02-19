/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.crashhelper;

import android.app.Service;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.Binder;
import android.os.DeadObjectException;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.util.Log;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.geckoview.BuildConfig;

public final class CrashHelper extends Service {
  private static final String LOGTAG = "GeckoCrashHelper";
  private static final boolean DEBUG = !BuildConfig.MOZILLA_OFFICIAL;
  private final Binder mBinder = new CrashHelperBinder();
  private static boolean sNativeLibLoaded;

  @Override
  public synchronized void onCreate() {
    if (!sNativeLibLoaded) {
      GeckoLoader.doLoadLibrary(this, "crashhelper");
      sNativeLibLoaded = true;
    }
  }

  private static class CrashHelperBinder extends ICrashHelper.Stub {
    @Override
    public boolean start(final int clientPid) {
      // Launch the crash helper code, this will spin out a thread which will
      // handle the IPC with Firefox' other processes. When running junit tests
      // we might have several main processes connecting to the service. Each
      // will get its own thread, but the generated crashes live in a shared
      // space. This means that technically one main process instance might
      // "steal" the crash report of another main process. We should add
      // additional separation within the crash generation code to prevent this
      // from happening even though it's very unlikely.
      CrashHelper.crash_generator(clientPid);

      return false;
    }
  }

  @Override
  public IBinder onBind(final Intent intent) {
    if (intent == null) {
      Log.d(LOGTAG, "Intent is empty, crash helper will not start");
      return null;
    }

    return mBinder;
  }

  public static ServiceConnection createConnection() {
    final ServiceConnection connection =
        new ServiceConnection() {
          @Override
          public void onServiceConnected(final ComponentName name, final IBinder service) {
            final ICrashHelper helper = ICrashHelper.Stub.asInterface(service);
            try {
              helper.start(Process.myPid());
            } catch (final DeadObjectException e) {
              // TODO: Cleanup?
              return;
            } catch (final RemoteException e) {
              throw new RuntimeException(e);
            }
          }

          @Override
          public void onServiceDisconnected(final ComponentName name) {
            // Nothing to do here
          }
        };

    return connection;
  }

  // Note: we don't implement onDestroy() because the service has the
  // `stopWithTask` flag set in the manifest, so the service manager will
  // tear it down for us.

  protected static native void crash_generator(int clientPid);
}
