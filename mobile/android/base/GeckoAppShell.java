/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.BufferedReader;
import java.io.Closeable;
import java.io.File;
import java.io.FileReader;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.net.URLConnection;
import java.net.MalformedURLException;
import java.net.Proxy;
import java.net.URL;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Queue;
import java.util.StringTokenizer;
import java.util.TreeMap;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

import org.mozilla.gecko.favicons.OnFaviconLoadedListener;
import org.mozilla.gecko.favicons.decoders.FaviconDecoder;
import org.mozilla.gecko.gfx.BitmapUtils;
import org.mozilla.gecko.gfx.LayerView;
import org.mozilla.gecko.gfx.PanZoomController;
import org.mozilla.gecko.mozglue.GeckoLoader;
import org.mozilla.gecko.mozglue.JNITarget;
import org.mozilla.gecko.mozglue.RobocopTarget;
import org.mozilla.gecko.mozglue.generatorannotations.OptionalGeneratedParameter;
import org.mozilla.gecko.mozglue.generatorannotations.WrapElementForJNI;
import org.mozilla.gecko.prompts.PromptService;
import org.mozilla.gecko.util.GeckoEventListener;
import org.mozilla.gecko.util.HardwareUtils;
import org.mozilla.gecko.util.NativeJSContainer;
import org.mozilla.gecko.util.ProxySelector;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.gecko.webapp.Allocator;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.PendingIntent;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.ImageFormat;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.hardware.Sensor;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.media.MediaScannerConnection;
import android.media.MediaScannerConnection.MediaScannerConnectionClient;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.MessageQueue;
import android.os.SystemClock;
import android.os.Vibrator;
import android.provider.Settings;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Base64;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.ContextThemeWrapper;
import android.view.HapticFeedbackConstants;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.webkit.MimeTypeMap;
import android.webkit.URLUtil;
import android.widget.AbsoluteLayout;
import android.widget.Toast;

public class GeckoAppShell
{
    private static final String LOGTAG = "GeckoAppShell";
    private static final boolean LOGGING = false;

    // We have static members only.
    private GeckoAppShell() { }

    private static boolean restartScheduled = false;
    private static GeckoEditableListener editableListener = null;

    private static final Queue<GeckoEvent> PENDING_EVENTS = new ConcurrentLinkedQueue<GeckoEvent>();
    private static final Map<String, String> ALERT_COOKIES = new ConcurrentHashMap<String, String>();

    private static volatile boolean locationHighAccuracyEnabled;

    // Accessed by NotificationHelper. This should be encapsulated.
    /* package */ static NotificationClient notificationClient;

    // See also HardwareUtils.LOW_MEMORY_THRESHOLD_MB.
    private static final int HIGH_MEMORY_DEVICE_THRESHOLD_MB = 768;

    public static final String SHORTCUT_TYPE_WEBAPP = "webapp";
    public static final String SHORTCUT_TYPE_BOOKMARK = "bookmark";

    static private int sDensityDpi = 0;
    static private int sScreenDepth = 0;

    /* Default colors. */
    private static final float[] DEFAULT_LAUNCHER_ICON_HSV = { 32.0f, 1.0f, 1.0f };

    /* Is the value in sVibrationEndTime valid? */
    private static boolean sVibrationMaybePlaying = false;

    /* Time (in System.nanoTime() units) when the currently-playing vibration
     * is scheduled to end.  This value is valid only when
     * sVibrationMaybePlaying is true. */
    private static long sVibrationEndTime = 0;

    /* Default value of how fast we should hint the Android sensors. */
    private static int sDefaultSensorHint = 100;

    private static Sensor gAccelerometerSensor = null;
    private static Sensor gLinearAccelerometerSensor = null;
    private static Sensor gGyroscopeSensor = null;
    private static Sensor gOrientationSensor = null;
    private static Sensor gProximitySensor = null;
    private static Sensor gLightSensor = null;

    /*
     * Keep in sync with constants found here:
     * http://mxr.mozilla.org/mozilla-central/source/uriloader/base/nsIWebProgressListener.idl
    */
    static public final int WPL_STATE_START = 0x00000001;
    static public final int WPL_STATE_STOP = 0x00000010;
    static public final int WPL_STATE_IS_DOCUMENT = 0x00020000;
    static public final int WPL_STATE_IS_NETWORK = 0x00040000;

    /* Keep in sync with constants found here:
      http://mxr.mozilla.org/mozilla-central/source/netwerk/base/public/nsINetworkLinkService.idl
    */
    static public final int LINK_TYPE_UNKNOWN = 0;
    static public final int LINK_TYPE_ETHERNET = 1;
    static public final int LINK_TYPE_USB = 2;
    static public final int LINK_TYPE_WIFI = 3;
    static public final int LINK_TYPE_WIMAX = 4;
    static public final int LINK_TYPE_2G = 5;
    static public final int LINK_TYPE_3G = 6;
    static public final int LINK_TYPE_4G = 7;

    /* The Android-side API: API methods that Android calls */

    // Initialization methods
    public static native void nativeInit();

    // helper methods
    //    public static native void setSurfaceView(GeckoSurfaceView sv);
    public static native void setLayerClient(Object client);
    public static native void onResume();
    public static void callObserver(String observerKey, String topic, String data) {
        sendEventToGecko(GeckoEvent.createCallObserverEvent(observerKey, topic, data));
    }
    public static void removeObserver(String observerKey) {
        sendEventToGecko(GeckoEvent.createRemoveObserverEvent(observerKey));
    }
    public static native Message getNextMessageFromQueue(MessageQueue queue);
    public static native void onSurfaceTextureFrameAvailable(Object surfaceTexture, int id);
    public static native void dispatchMemoryPressure();

    public static void registerGlobalExceptionHandler() {
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread thread, Throwable e) {
                handleUncaughtException(thread, e);
            }
        });
    }

    private static String getStackTraceString(Throwable e) {
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        e.printStackTrace(pw);
        pw.flush();
        return sw.toString();
    }

    private static native void reportJavaCrash(String stackTrace);

    public static void notifyUriVisited(String uri) {
        sendEventToGecko(GeckoEvent.createVisitedEvent(uri));
    }

    public static native void processNextNativeEvent(boolean mayWait);

    public static native void notifyBatteryChange(double aLevel, boolean aCharging, double aRemainingTime);

    public static native void scheduleComposite();

    // Resuming the compositor is a synchronous request, so be
    // careful of possible deadlock. Resuming the compositor will also cause
    // a composition, so there is no need to schedule a composition after
    // resuming.
    public static native void scheduleResumeComposition(int width, int height);

    public static native float computeRenderIntegrity();

    public static native SurfaceBits getSurfaceBits(Surface surface);

    public static native void onFullScreenPluginHidden(View view);

    public static class CreateShortcutFaviconLoadedListener implements OnFaviconLoadedListener {
        private final String title;
        private final String url;

        public CreateShortcutFaviconLoadedListener(final String url, final String title) {
            this.url = url;
            this.title = title;
        }

        @Override
        public void onFaviconLoaded(String pageUrl, String faviconURL, Bitmap favicon) {
            GeckoAppShell.createShortcut(title, url, url, favicon, "");
        }
    }

    private static final class GeckoMediaScannerClient implements MediaScannerConnectionClient {
        private final String mFile;
        private final String mMimeType;
        private MediaScannerConnection mScanner;

        public static void startScan(Context context, String file, String mimeType) {
            new GeckoMediaScannerClient(context, file, mimeType);
        }

        private GeckoMediaScannerClient(Context context, String file, String mimeType) {
            mFile = file;
            mMimeType = mimeType;
            mScanner = new MediaScannerConnection(context, this);
            mScanner.connect();
        }

        @Override
        public void onMediaScannerConnected() {
            mScanner.scanFile(mFile, mMimeType);
        }

        @Override
        public void onScanCompleted(String path, Uri uri) {
            if(path.equals(mFile)) {
                mScanner.disconnect();
                mScanner = null;
            }
        }
    }

    private static LayerView sLayerView;

    public static void setLayerView(LayerView lv) {
        sLayerView = lv;
    }

    @RobocopTarget
    public static LayerView getLayerView() {
        return sLayerView;
    }

    public static void runGecko(String apkPath, String args, String url, String type) {
        // Preparation for pumpMessageLoop()
        MessageQueue.IdleHandler idleHandler = new MessageQueue.IdleHandler() {
            @Override public boolean queueIdle() {
                final Handler geckoHandler = ThreadUtils.sGeckoHandler;
                Message idleMsg = Message.obtain(geckoHandler);
                // Use |Message.obj == GeckoHandler| to identify our "queue is empty" message
                idleMsg.obj = geckoHandler;
                geckoHandler.sendMessageAtFrontOfQueue(idleMsg);
                // Keep this IdleHandler
                return true;
            }
        };
        Looper.myQueue().addIdleHandler(idleHandler);

        // run gecko -- it will spawn its own thread
        GeckoAppShell.nativeInit();

        if (sLayerView != null)
            GeckoAppShell.setLayerClient(sLayerView.getLayerClientObject());

        // First argument is the .apk path
        String combinedArgs = apkPath + " -greomni " + apkPath;
        if (args != null)
            combinedArgs += " " + args;
        if (url != null)
            combinedArgs += " -url " + url;
        if (type != null)
            combinedArgs += " " + type;

        // In un-official builds, we want to load Javascript resources fresh
        // with each build.  In official builds, the startup cache is purged by
        // the buildid mechanism, but most un-official builds don't bump the
        // buildid, so we purge here instead.
        if (!AppConstants.MOZILLA_OFFICIAL) {
            Log.w(LOGTAG, "STARTUP PERFORMANCE WARNING: un-official build: purging the " +
                          "startup (JavaScript) caches.");
            combinedArgs += " -purgecaches";
        }

        DisplayMetrics metrics = getContext().getResources().getDisplayMetrics();
        combinedArgs += " -width " + metrics.widthPixels + " -height " + metrics.heightPixels;

        ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    geckoLoaded();
                }
            });

        if (!AppConstants.MOZILLA_OFFICIAL) {
            Log.d(LOGTAG, "GeckoLoader.nativeRun " + combinedArgs);
        }
        // and go
        GeckoLoader.nativeRun(combinedArgs);

        // Remove pumpMessageLoop() idle handler
        Looper.myQueue().removeIdleHandler(idleHandler);
    }

    // Called on the UI thread after Gecko loads.
    private static void geckoLoaded() {
        GeckoEditable editable = new GeckoEditable();
        // install the gecko => editable listener
        editableListener = editable;
    }

    static void sendPendingEventsToGecko() {
        try {
            while (!PENDING_EVENTS.isEmpty()) {
                final GeckoEvent e = PENDING_EVENTS.poll();
                notifyGeckoOfEvent(e);
            }
        } catch (NoSuchElementException e) {}
    }

    /**
     * If the Gecko thread is running, immediately dispatches the event to
     * Gecko.
     *
     * If the Gecko thread is not running, queues the event. If the queue is
     * full, throws {@link IllegalStateException}.
     *
     * Queued events will be dispatched in order of arrival when the Gecko
     * thread becomes live.
     *
     * This method can be called from any thread.
     *
     * @param e
     *            the event to dispatch. Cannot be null.
     */
    @RobocopTarget
    public static void sendEventToGecko(GeckoEvent e) {
        if (e == null) {
            throw new IllegalArgumentException("e cannot be null.");
        }

        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoRunning)) {
            notifyGeckoOfEvent(e);
            // Gecko will copy the event data into a normal C++ object. We can recycle the event now.
            e.recycle();
            return;
        }

        // Throws if unable to add the event due to capacity restrictions.
        PENDING_EVENTS.add(e);
    }

    // Tell the Gecko event loop that an event is available.
    public static native void notifyGeckoOfEvent(GeckoEvent event);

    /*
     *  The Gecko-side API: API methods that Gecko calls
     */

    @WrapElementForJNI(allowMultithread = true, generateStatic = true, noThrow = true)
    public static void handleUncaughtException(Thread thread, Throwable e) {
        if (GeckoThread.checkLaunchState(GeckoThread.LaunchState.GeckoExited)) {
            // We've called System.exit. All exceptions after this point are Android
            // berating us for being nasty to it.
            return;
        }

        if (thread == null) {
            thread = Thread.currentThread();
        }
        // If the uncaught exception was rethrown, walk the exception `cause` chain to find
        // the original exception so Socorro can correctly collate related crash reports.
        Throwable cause;
        while ((cause = e.getCause()) != null) {
            e = cause;
        }

        try {
            Log.e(LOGTAG, ">>> REPORTING UNCAUGHT EXCEPTION FROM THREAD "
                          + thread.getId() + " (\"" + thread.getName() + "\")", e);

            Thread mainThread = ThreadUtils.getUiThread();
            if (mainThread != null && thread != mainThread) {
                Log.e(LOGTAG, "Main thread stack:");
                for (StackTraceElement ste : mainThread.getStackTrace()) {
                    Log.e(LOGTAG, ste.toString());
                }
            }

            if (e instanceof OutOfMemoryError) {
                SharedPreferences prefs = getSharedPreferences();
                SharedPreferences.Editor editor = prefs.edit();
                editor.putBoolean(GeckoApp.PREFS_OOM_EXCEPTION, true);
                editor.commit();
            }
        } finally {
            reportJavaCrash(getStackTraceString(e));
        }
    }

    @WrapElementForJNI(generateStatic = true)
    public static void notifyIME(int type) {
        if (editableListener != null) {
            editableListener.notifyIME(type);
        }
    }

    @WrapElementForJNI(generateStatic = true)
    public static void notifyIMEContext(int state, String typeHint,
                                        String modeHint, String actionHint) {
        if (editableListener != null) {
            editableListener.notifyIMEContext(state, typeHint,
                                               modeHint, actionHint);
        }
    }

    @WrapElementForJNI(generateStatic = true)
    public static void notifyIMEChange(String text, int start, int end, int newEnd) {
        if (newEnd < 0) { // Selection change
            editableListener.onSelectionChange(start, end);
        } else { // Text change
            editableListener.onTextChange(text, start, end, newEnd);
        }
    }

    private static final Object sEventAckLock = new Object();
    private static boolean sWaitingForEventAck;

    // Block the current thread until the Gecko event loop is caught up
    public static void sendEventToGeckoSync(GeckoEvent e) {
        e.setAckNeeded(true);

        long time = SystemClock.uptimeMillis();
        boolean isUiThread = ThreadUtils.isOnUiThread();

        synchronized (sEventAckLock) {
            if (sWaitingForEventAck) {
                // should never happen since we always leave it as false when we exit this function.
                Log.e(LOGTAG, "geckoEventSync() may have been called twice concurrently!", new Exception());
                // fall through for graceful handling
            }

            sendEventToGecko(e);
            sWaitingForEventAck = true;
            while (true) {
                try {
                    sEventAckLock.wait(1000);
                } catch (InterruptedException ie) {
                }
                if (!sWaitingForEventAck) {
                    // response received
                    break;
                }
                long waited = SystemClock.uptimeMillis() - time;
                Log.d(LOGTAG, "Gecko event sync taking too long: " + waited + "ms");
            }
        }
    }

    // Signal the Java thread that it's time to wake up
    @WrapElementForJNI
    public static void acknowledgeEvent() {
        synchronized (sEventAckLock) {
            sWaitingForEventAck = false;
            sEventAckLock.notifyAll();
        }
    }

    private static float getLocationAccuracy(Location location) {
        float radius = location.getAccuracy();
        return (location.hasAccuracy() && radius > 0) ? radius : 1001;
    }

    private static Location getLastKnownLocation(LocationManager lm) {
        Location lastKnownLocation = null;
        List<String> providers = lm.getAllProviders();

        for (String provider : providers) {
            Location location = lm.getLastKnownLocation(provider);
            if (location == null) {
                continue;
            }

            if (lastKnownLocation == null) {
                lastKnownLocation = location;
                continue;
            }

            long timeDiff = location.getTime() - lastKnownLocation.getTime();
            if (timeDiff > 0 ||
                (timeDiff == 0 &&
                 getLocationAccuracy(location) < getLocationAccuracy(lastKnownLocation))) {
                lastKnownLocation = location;
            }
        }

        return lastKnownLocation;
    }

    @WrapElementForJNI
    public static void enableLocation(final boolean enable) {
        ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    LocationManager lm = getLocationManager(getContext());
                    if (lm == null) {
                        return;
                    }

                    if (enable) {
                        Location lastKnownLocation = getLastKnownLocation(lm);
                        if (lastKnownLocation != null) {
                            getGeckoInterface().getLocationListener().onLocationChanged(lastKnownLocation);
                        }

                        Criteria criteria = new Criteria();
                        criteria.setSpeedRequired(false);
                        criteria.setBearingRequired(false);
                        criteria.setAltitudeRequired(false);
                        if (locationHighAccuracyEnabled) {
                            criteria.setAccuracy(Criteria.ACCURACY_FINE);
                            criteria.setCostAllowed(true);
                            criteria.setPowerRequirement(Criteria.POWER_HIGH);
                        } else {
                            criteria.setAccuracy(Criteria.ACCURACY_COARSE);
                            criteria.setCostAllowed(false);
                            criteria.setPowerRequirement(Criteria.POWER_LOW);
                        }

                        String provider = lm.getBestProvider(criteria, true);
                        if (provider == null)
                            return;

                        Looper l = Looper.getMainLooper();
                        lm.requestLocationUpdates(provider, 100, (float).5, getGeckoInterface().getLocationListener(), l);
                    } else {
                        lm.removeUpdates(getGeckoInterface().getLocationListener());
                    }
                }
            });
    }

    private static LocationManager getLocationManager(Context context) {
        try {
            return (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
        } catch (NoSuchFieldError e) {
            // Some Tegras throw exceptions about missing the CONTROL_LOCATION_UPDATES permission,
            // which allows enabling/disabling location update notifications from the cell radio.
            // CONTROL_LOCATION_UPDATES is not for use by normal applications, but we might be
            // hitting this problem if the Tegras are confused about missing cell radios.
            Log.e(LOGTAG, "LOCATION_SERVICE not found?!", e);
            return null;
        }
    }

    @WrapElementForJNI
    public static void enableLocationHighAccuracy(final boolean enable) {
        locationHighAccuracyEnabled = enable;
    }

    @WrapElementForJNI
    public static void enableSensor(int aSensortype) {
        GeckoInterface gi = getGeckoInterface();
        if (gi == null)
            return;
        SensorManager sm = (SensorManager)
            getContext().getSystemService(Context.SENSOR_SERVICE);

        switch(aSensortype) {
        case GeckoHalDefines.SENSOR_ORIENTATION:
            if(gOrientationSensor == null)
                gOrientationSensor = sm.getDefaultSensor(Sensor.TYPE_ORIENTATION);
            if (gOrientationSensor != null) 
                sm.registerListener(gi.getSensorEventListener(), gOrientationSensor, sDefaultSensorHint);
            break;

        case GeckoHalDefines.SENSOR_ACCELERATION:
            if(gAccelerometerSensor == null)
                gAccelerometerSensor = sm.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
            if (gAccelerometerSensor != null)
                sm.registerListener(gi.getSensorEventListener(), gAccelerometerSensor, sDefaultSensorHint);
            break;

        case GeckoHalDefines.SENSOR_PROXIMITY:
            if(gProximitySensor == null  )
                gProximitySensor = sm.getDefaultSensor(Sensor.TYPE_PROXIMITY);
            if (gProximitySensor != null)
                sm.registerListener(gi.getSensorEventListener(), gProximitySensor, SensorManager.SENSOR_DELAY_NORMAL);
            break;

        case GeckoHalDefines.SENSOR_LIGHT:
            if(gLightSensor == null)
                gLightSensor = sm.getDefaultSensor(Sensor.TYPE_LIGHT);
            if (gLightSensor != null)
                sm.registerListener(gi.getSensorEventListener(), gLightSensor, SensorManager.SENSOR_DELAY_NORMAL);
            break;

        case GeckoHalDefines.SENSOR_LINEAR_ACCELERATION:
            if(gLinearAccelerometerSensor == null)
                gLinearAccelerometerSensor = sm.getDefaultSensor(10 /* API Level 9 - TYPE_LINEAR_ACCELERATION */);
            if (gLinearAccelerometerSensor != null)
                sm.registerListener(gi.getSensorEventListener(), gLinearAccelerometerSensor, sDefaultSensorHint);
            break;

        case GeckoHalDefines.SENSOR_GYROSCOPE:
            if(gGyroscopeSensor == null)
                gGyroscopeSensor = sm.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
            if (gGyroscopeSensor != null)
                sm.registerListener(gi.getSensorEventListener(), gGyroscopeSensor, sDefaultSensorHint);
            break;
        default:
            Log.w(LOGTAG, "Error! Can't enable unknown SENSOR type " + aSensortype);
        }
    }

    @WrapElementForJNI
    public static void disableSensor(int aSensortype) {
        GeckoInterface gi = getGeckoInterface();
        if (gi == null)
            return;

        SensorManager sm = (SensorManager)
            getContext().getSystemService(Context.SENSOR_SERVICE);

        switch (aSensortype) {
        case GeckoHalDefines.SENSOR_ORIENTATION:
            if (gOrientationSensor != null)
                sm.unregisterListener(gi.getSensorEventListener(), gOrientationSensor);
            break;

        case GeckoHalDefines.SENSOR_ACCELERATION:
            if (gAccelerometerSensor != null)
                sm.unregisterListener(gi.getSensorEventListener(), gAccelerometerSensor);
            break;

        case GeckoHalDefines.SENSOR_PROXIMITY:
            if (gProximitySensor != null)
                sm.unregisterListener(gi.getSensorEventListener(), gProximitySensor);
            break;

        case GeckoHalDefines.SENSOR_LIGHT:
            if (gLightSensor != null)
                sm.unregisterListener(gi.getSensorEventListener(), gLightSensor);
            break;

        case GeckoHalDefines.SENSOR_LINEAR_ACCELERATION:
            if (gLinearAccelerometerSensor != null)
                sm.unregisterListener(gi.getSensorEventListener(), gLinearAccelerometerSensor);
            break;

        case GeckoHalDefines.SENSOR_GYROSCOPE:
            if (gGyroscopeSensor != null)
                sm.unregisterListener(gi.getSensorEventListener(), gGyroscopeSensor);
            break;
        default:
            Log.w(LOGTAG, "Error! Can't disable unknown SENSOR type " + aSensortype);
        }
    }

    @WrapElementForJNI
    public static void startMonitoringGamepad() {
        if (Build.VERSION.SDK_INT < 9) {
            return;
        }
        ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    AndroidGamepadManager.startup();
                }
            });
    }

    @WrapElementForJNI
    public static void stopMonitoringGamepad() {
        if (Build.VERSION.SDK_INT < 9) {
            return;
        }

        ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    AndroidGamepadManager.shutdown();
                }
            });
    }

    @WrapElementForJNI
    public static void gamepadAdded(final int device_id, final int service_id) {
        ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    AndroidGamepadManager.gamepadAdded(device_id, service_id);
                }
            });
    }

    @WrapElementForJNI
    public static void moveTaskToBack() {
        if (getGeckoInterface() != null)
            getGeckoInterface().getActivity().moveTaskToBack(true);
    }

    public static void returnIMEQueryResult(String result, int selectionStart, int selectionLength) {
        // This method may be called from JNI to report Gecko's current selection indexes, but
        // Native Fennec doesn't care because the Java code already knows the selection indexes.
    }

    @WrapElementForJNI(stubName = "NotifyXreExit")
    static void onXreExit() {
        // The launch state can only be Launched or GeckoRunning at this point
        GeckoThread.setLaunchState(GeckoThread.LaunchState.GeckoExiting);
        if (getGeckoInterface() != null) {
            if (restartScheduled) {
                getGeckoInterface().doRestart();
            } else {
                getGeckoInterface().getActivity().finish();
            }
        }

        systemExit();
    }

    static void systemExit() {
        Log.d(LOGTAG, "Killing via System.exit()");
        GeckoThread.setLaunchState(GeckoThread.LaunchState.GeckoExited);
        System.exit(0);
    }

    @WrapElementForJNI
    static void scheduleRestart() {
        restartScheduled = true;
    }

    public static Intent getWebappIntent(String aURI, String aOrigin, String aTitle, Bitmap aIcon) {
        Intent intent;

        Allocator slots = Allocator.getInstance(getContext());
        int index = slots.getIndexForOrigin(aOrigin);

        if (index == -1) {
            return null;
        }

        String packageName = slots.getAppForIndex(index);
        intent = getContext().getPackageManager().getLaunchIntentForPackage(packageName);
        if (aURI != null) {
            intent.setData(Uri.parse(aURI));
        }

        return intent;
    }

    // "Installs" an application by creating a shortcut
    // This is the entry point from AndroidBridge.h
    @WrapElementForJNI
    static void createShortcut(String aTitle, String aURI, String aIconData, String aType) {
        if ("webapp".equals(aType)) {
            Log.w(LOGTAG, "createShortcut with no unique URI should not be used for aType = webapp!");
        }

        createShortcut(aTitle, aURI, aURI, aIconData, aType);
    }

    // For non-webapps.
    public static void createShortcut(String aTitle, String aURI, Bitmap aBitmap, String aType) {
        createShortcut(aTitle, aURI, aURI, aBitmap, aType);
    }

    // Internal, for webapps.
    static void createShortcut(final String aTitle, final String aURI, final String aUniqueURI, final String aIconData, final String aType) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                // TODO: use the cache. Bug 961600.
                Bitmap icon = FaviconDecoder.getMostSuitableBitmapFromDataURI(aIconData, getPreferredIconSize());
                GeckoAppShell.doCreateShortcut(aTitle, aURI, aURI, icon, aType);
            }
        });
    }

    public static void createShortcut(final String aTitle, final String aURI, final String aUniqueURI,
                                      final Bitmap aIcon, final String aType) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                GeckoAppShell.doCreateShortcut(aTitle, aURI, aUniqueURI, aIcon, aType);
            }
        });
    }

    /**
     * Call this method only on the background thread.
     */
    private static void doCreateShortcut(final String aTitle, final String aURI, final String aUniqueURI,
                                         final Bitmap aIcon, final String aType) {
        // The intent to be launched by the shortcut.
        Intent shortcutIntent;
        if (aType.equalsIgnoreCase(SHORTCUT_TYPE_WEBAPP)) {
            shortcutIntent = getWebappIntent(aURI, aUniqueURI, aTitle, aIcon);
        } else {
            shortcutIntent = new Intent();
            shortcutIntent.setAction(GeckoApp.ACTION_HOMESCREEN_SHORTCUT);
            shortcutIntent.setData(Uri.parse(aURI));
            shortcutIntent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                                        AppConstants.BROWSER_INTENT_CLASS_NAME);
        }

        Intent intent = new Intent();
        intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
        intent.putExtra(Intent.EXTRA_SHORTCUT_ICON, getLauncherIcon(aIcon, aType));

        if (aTitle != null) {
            intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aTitle);
        } else {
            intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aURI);
        }

        // Do not allow duplicate items.
        intent.putExtra("duplicate", false);

        intent.setAction("com.android.launcher.action.INSTALL_SHORTCUT");
        getContext().sendBroadcast(intent);
    }

    public static void removeShortcut(final String aTitle, final String aURI, final String aType) {
        removeShortcut(aTitle, aURI, null, aType);
    }

    public static void removeShortcut(final String aTitle, final String aURI, final String aUniqueURI, final String aType) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                // the intent to be launched by the shortcut
                Intent shortcutIntent;
                if (aType.equalsIgnoreCase(SHORTCUT_TYPE_WEBAPP)) {
                    shortcutIntent = getWebappIntent(aURI, aUniqueURI, "", null);
                    if (shortcutIntent == null)
                        return;
                } else {
                    shortcutIntent = new Intent();
                    shortcutIntent.setAction(GeckoApp.ACTION_HOMESCREEN_SHORTCUT);
                    shortcutIntent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                                                AppConstants.BROWSER_INTENT_CLASS_NAME);
                    shortcutIntent.setData(Uri.parse(aURI));
                }
        
                Intent intent = new Intent();
                intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
                if (aTitle != null)
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aTitle);
                else
                    intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, aURI);

                intent.setAction("com.android.launcher.action.UNINSTALL_SHORTCUT");
                getContext().sendBroadcast(intent);
            }
        });
    }

    @JNITarget
    static public int getPreferredIconSize() {
        if (android.os.Build.VERSION.SDK_INT >= 11) {
            ActivityManager am = (ActivityManager)getContext().getSystemService(Context.ACTIVITY_SERVICE);
            return am.getLauncherLargeIconSize();
        } else {
            switch (getDpi()) {
                case DisplayMetrics.DENSITY_MEDIUM:
                    return 48;
                case DisplayMetrics.DENSITY_XHIGH:
                    return 96;
                case DisplayMetrics.DENSITY_HIGH:
                default:
                    return 72;
            }
        }
    }

    static private Bitmap getLauncherIcon(Bitmap aSource, String aType) {
        final int kOffset = 6;
        final int kRadius = 5;
        int size = getPreferredIconSize();
        int insetSize = aSource != null ? size * 2 / 3 : size;

        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);


        // draw a base color
        Paint paint = new Paint();
        if (aSource == null) {
            // If we aren't drawing a favicon, just use an orange color.
            paint.setColor(Color.HSVToColor(DEFAULT_LAUNCHER_ICON_HSV));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset), kRadius, kRadius, paint);
        } else if (aType.equalsIgnoreCase(SHORTCUT_TYPE_WEBAPP) || aSource.getWidth() >= insetSize || aSource.getHeight() >= insetSize) {
            // otherwise, if this is a webapp or if the icons is lare enough, just draw it
            Rect iconBounds = new Rect(0, 0, size, size);
            canvas.drawBitmap(aSource, null, iconBounds, null);
            return bitmap;
        } else {
            // otherwise use the dominant color from the icon + a layer of transparent white to lighten it somewhat
            int color = BitmapUtils.getDominantColor(aSource);
            paint.setColor(color);
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset), kRadius, kRadius, paint);
            paint.setColor(Color.argb(100, 255, 255, 255));
            canvas.drawRoundRect(new RectF(kOffset, kOffset, size - kOffset, size - kOffset), kRadius, kRadius, paint);
        }

        // draw the overlay
        Bitmap overlay = BitmapUtils.decodeResource(getContext(), R.drawable.home_bg);
        canvas.drawBitmap(overlay, null, new Rect(0, 0, size, size), null);

        // draw the favicon
        if (aSource == null)
            aSource = BitmapUtils.decodeResource(getContext(), R.drawable.home_star);

        // by default, we scale the icon to this size
        int sWidth = insetSize / 2;
        int sHeight = sWidth;

        int halfSize = size / 2;
        canvas.drawBitmap(aSource,
                          null,
                          new Rect(halfSize - sWidth,
                                   halfSize - sHeight,
                                   halfSize + sWidth,
                                   halfSize + sHeight),
                          null);

        return bitmap;
    }

    @WrapElementForJNI(stubName = "GetHandlersForMimeTypeWrapper")
    static String[] getHandlersForMimeType(String aMimeType, String aAction) {
        Intent intent = getIntentForActionString(aAction);
        if (aMimeType != null && aMimeType.length() > 0)
            intent.setType(aMimeType);
        return getHandlersForIntent(intent);
    }

    @WrapElementForJNI(stubName = "GetHandlersForURLWrapper")
    static String[] getHandlersForURL(String aURL, String aAction) {
        // aURL may contain the whole URL or just the protocol
        Uri uri = aURL.indexOf(':') >= 0 ? Uri.parse(aURL) : new Uri.Builder().scheme(aURL).build();

        Intent intent = getOpenURIIntent(getContext(), uri.toString(), "",
            TextUtils.isEmpty(aAction) ? Intent.ACTION_VIEW : aAction, "");

        return getHandlersForIntent(intent);
    }

    static boolean hasHandlersForIntent(Intent intent) {
        PackageManager pm = getContext().getPackageManager();
        List<ResolveInfo> list = pm.queryIntentActivities(intent, 0);
        return !list.isEmpty();
    }

    static String[] getHandlersForIntent(Intent intent) {
        PackageManager pm = getContext().getPackageManager();
        List<ResolveInfo> list = pm.queryIntentActivities(intent, 0);
        int numAttr = 4;
        String[] ret = new String[list.size() * numAttr];
        for (int i = 0; i < list.size(); i++) {
            ResolveInfo resolveInfo = list.get(i);
            ret[i * numAttr] = resolveInfo.loadLabel(pm).toString();
            if (resolveInfo.isDefault)
                ret[i * numAttr + 1] = "default";
            else
                ret[i * numAttr + 1] = "";
            ret[i * numAttr + 2] = resolveInfo.activityInfo.applicationInfo.packageName;
            ret[i * numAttr + 3] = resolveInfo.activityInfo.name;
        }
        return ret;
    }

    static Intent getIntentForActionString(String aAction) {
        // Default to the view action if no other action as been specified.
        if (TextUtils.isEmpty(aAction)) {
            return new Intent(Intent.ACTION_VIEW);
        }
        return new Intent(aAction);
    }

    @WrapElementForJNI(stubName = "GetExtensionFromMimeTypeWrapper")
    static String getExtensionFromMimeType(String aMimeType) {
        return MimeTypeMap.getSingleton().getExtensionFromMimeType(aMimeType);
    }

    @WrapElementForJNI(stubName = "GetMimeTypeFromExtensionsWrapper")
    static String getMimeTypeFromExtensions(String aFileExt) {
        StringTokenizer st = new StringTokenizer(aFileExt, ".,; ");
        String type = null;
        String subType = null;
        while (st.hasMoreElements()) {
            String ext = st.nextToken();
            String mt = getMimeTypeFromExtension(ext);
            if (mt == null)
                continue;
            int slash = mt.indexOf('/');
            String tmpType = mt.substring(0, slash);
            if (!tmpType.equalsIgnoreCase(type))
                type = type == null ? tmpType : "*";
            String tmpSubType = mt.substring(slash + 1);
            if (!tmpSubType.equalsIgnoreCase(subType))
                subType = subType == null ? tmpSubType : "*";
        }
        if (type == null)
            type = "*";
        if (subType == null)
            subType = "*";
        return type + "/" + subType;
    }

    static void safeStreamClose(Closeable stream) {
        try {
            if (stream != null)
                stream.close();
        } catch (IOException e) {}
    }

    static boolean isUriSafeForScheme(Uri aUri) {
        // Bug 794034 - We don't want to pass MWI or USSD codes to the
        // dialer, and ensure the Uri class doesn't parse a URI
        // containing a fragment ('#')
        final String scheme = aUri.getScheme();
        if ("tel".equals(scheme) || "sms".equals(scheme)) {
            final String number = aUri.getSchemeSpecificPart();
            if (number.contains("#") || number.contains("*") || aUri.getFragment() != null) {
                return false;
            }
        }
        return true;
    }

    /**
     * Given the inputs to <code>getOpenURIIntent</code>, plus an optional
     * package name and class name, create and fire an intent to open the
     * provided URI. If a class name is specified but a package name is not,
     * we will default to using the current fennec package.
     *
     * @param targetURI the string spec of the URI to open.
     * @param mimeType an optional MIME type string.
     * @param packageName an optional app package name.
     * @param className an optional intent class name.
     * @param action an Android action specifier, such as
     *               <code>Intent.ACTION_SEND</code>.
     * @param title the title to use in <code>ACTION_SEND</code> intents.
     * @return true if the activity started successfully; false otherwise.
     */
    @WrapElementForJNI
    public static boolean openUriExternal(String targetURI,
                                          String mimeType,
              @OptionalGeneratedParameter String packageName,
              @OptionalGeneratedParameter String className,
              @OptionalGeneratedParameter String action,
              @OptionalGeneratedParameter String title) {
        final Context context = getContext();
        final Intent intent = getOpenURIIntent(context, targetURI,
                                               mimeType, action, title);

        if (intent == null) {
            return false;
        }

        if (!TextUtils.isEmpty(className)) {
            if (!TextUtils.isEmpty(packageName)) {
                intent.setClassName(packageName, className);
            } else {
                // Default to using the fennec app context.
                intent.setClassName(context, className);
            }
        }

        intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        try {
            context.startActivity(intent);
            return true;
        } catch (ActivityNotFoundException e) {
            return false;
        }
    }

    /**
     * Return a <code>Uri</code> instance which is equivalent to <code>u</code>,
     * but with a guaranteed-lowercase scheme as if the API level 16 method
     * <code>u.normalizeScheme</code> had been called.
     *
     * @param u the <code>Uri</code> to normalize.
     * @return a <code>Uri</code>, which might be <code>u</code>.
     */
    static Uri normalizeUriScheme(final Uri u) {
        final String scheme = u.getScheme();
        final String lower  = scheme.toLowerCase(Locale.US);
        if (lower.equals(scheme)) {
            return u;
        }

        // Otherwise, return a new URI with a normalized scheme.
        return u.buildUpon().scheme(lower).build();
    }

    /**
     * Given a URI, a MIME type, and a title,
     * produce a share intent which can be used to query all activities
     * than can open the specified URI.
     *
     * @param context a <code>Context</code> instance.
     * @param targetURI the string spec of the URI to open.
     * @param mimeType an optional MIME type string.
     * @param title the title to use in <code>ACTION_SEND</code> intents.
     * @return an <code>Intent</code>, or <code>null</code> if none could be
     *         produced.
     */
    public static Intent getShareIntent(final Context context,
                                        final String targetURI,
                                        final String mimeType,
                                        final String title) {
        Intent shareIntent = getIntentForActionString(Intent.ACTION_SEND);
        shareIntent.putExtra(Intent.EXTRA_TEXT, targetURI);
        shareIntent.putExtra(Intent.EXTRA_SUBJECT, title);

        // Note that EXTRA_TITLE is intended to be used for share dialog
        // titles. Common usage (e.g., Pocket) suggests that it's sometimes
        // interpreted as an alternate to EXTRA_SUBJECT, so we include it.
        shareIntent.putExtra(Intent.EXTRA_TITLE, title);

        if (mimeType != null && mimeType.length() > 0) {
            shareIntent.setType(mimeType);
        }

        return shareIntent;
    }

    /**
     * Given a URI, a MIME type, an Android intent "action", and a title,
     * produce an intent which can be used to start an activity to open
     * the specified URI.
     *
     * @param context a <code>Context</code> instance.
     * @param targetURI the string spec of the URI to open.
     * @param mimeType an optional MIME type string.
     * @param action an Android action specifier, such as
     *               <code>Intent.ACTION_SEND</code>.
     * @param title the title to use in <code>ACTION_SEND</code> intents.
     * @return an <code>Intent</code>, or <code>null</code> if none could be
     *         produced.
     */
    static Intent getOpenURIIntent(final Context context,
                                   final String targetURI,
                                   final String mimeType,
                                   final String action,
                                   final String title) {

        if (action.equalsIgnoreCase(Intent.ACTION_SEND)) {
            Intent shareIntent = getShareIntent(context, targetURI, mimeType, title);
            return Intent.createChooser(shareIntent,
                                        context.getResources().getString(R.string.share_title)); 
        }

        final Uri uri = normalizeUriScheme(targetURI.indexOf(':') >= 0 ? Uri.parse(targetURI) : new Uri.Builder().scheme(targetURI).build());
        if (mimeType.length() > 0) {
            Intent intent = getIntentForActionString(action);
            intent.setDataAndType(uri, mimeType);
            return intent;
        }

        if (!isUriSafeForScheme(uri)) {
            return null;
        }

        final String scheme = uri.getScheme();

        final Intent intent;

        // Compute our most likely intent, then check to see if there are any
        // custom handlers that would apply.
        // Start with the original URI. If we end up modifying it, we'll
        // overwrite it.
        final Intent likelyIntent = getIntentForActionString(action);
        likelyIntent.setData(uri);

        if ("vnd.youtube".equals(scheme) && !hasHandlersForIntent(likelyIntent)) {
            // Special-case YouTube to use our own player if no system handler
            // exists.
            intent = new Intent(VideoPlayer.VIDEO_ACTION);
            intent.setClassName(AppConstants.ANDROID_PACKAGE_NAME,
                                "org.mozilla.gecko.VideoPlayer");
            intent.setData(uri);
        } else {
            intent = likelyIntent;
        }

        // Have a special handling for SMS, as the message body
        // is not extracted from the URI automatically.
        if (!"sms".equals(scheme)) {
            return intent;
        }

        final String query = uri.getEncodedQuery();
        if (TextUtils.isEmpty(query)) {
            return intent;
        }

        final String[] fields = query.split("&");
        boolean foundBody = false;
        String resultQuery = "";
        for (String field : fields) {
            if (foundBody || !field.startsWith("body=")) {
                resultQuery = resultQuery.concat(resultQuery.length() > 0 ? "&" + field : field);
                continue;
            }

            // Found the first body param. Put it into the intent.
            final String body = Uri.decode(field.substring(5));
            intent.putExtra("sms_body", body);
            foundBody = true;
        }

        if (!foundBody) {
            // No need to rewrite the URI, then.
            return intent;
        }

        // Form a new URI without the body field in the query part, and
        // push that into the new Intent.
        final String newQuery = resultQuery.length() > 0 ? "?" + resultQuery : "";
        final Uri pruned = uri.buildUpon().encodedQuery(newQuery).build();
        intent.setData(pruned);

        return intent;
    }

    /**
     * Only called from GeckoApp.
     */
    public static void setNotificationClient(NotificationClient client) {
        if (notificationClient == null) {
            notificationClient = client;
        } else {
            Log.d(LOGTAG, "Notification client already set");
        }
    }

    @WrapElementForJNI(stubName = "ShowAlertNotificationWrapper")
    public static void showAlertNotification(String aImageUrl, String aAlertTitle, String aAlertText,
                                             String aAlertCookie, String aAlertName) {
        // The intent to launch when the user clicks the expanded notification
        String app = getContext().getClass().getName();
        Intent notificationIntent = new Intent(GeckoApp.ACTION_ALERT_CALLBACK);
        notificationIntent.setClassName(AppConstants.ANDROID_PACKAGE_NAME, app);
        notificationIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        int notificationID = aAlertName.hashCode();

        // Put the strings into the intent as an URI "alert:?name=<alertName>&app=<appName>&cookie=<cookie>"
        Uri.Builder b = new Uri.Builder();
        Uri dataUri = b.scheme("alert").path(Integer.toString(notificationID))
                                       .appendQueryParameter("name", aAlertName)
                                       .appendQueryParameter("cookie", aAlertCookie)
                                       .build();
        notificationIntent.setData(dataUri);
        PendingIntent contentIntent = PendingIntent.getActivity(
                getContext(), 0, notificationIntent, PendingIntent.FLAG_UPDATE_CURRENT);

        ALERT_COOKIES.put(aAlertName, aAlertCookie);
        callObserver(aAlertName, "alertshow", aAlertCookie);

        notificationClient.add(notificationID, aImageUrl, aAlertTitle, aAlertText, contentIntent);
    }

    @WrapElementForJNI
    public static void alertsProgressListener_OnProgress(String aAlertName, long aProgress, long aProgressMax, String aAlertText) {
        int notificationID = aAlertName.hashCode();
        notificationClient.update(notificationID, aProgress, aProgressMax, aAlertText);
    }

    @WrapElementForJNI
    public static void closeNotification(String aAlertName) {
        String alertCookie = ALERT_COOKIES.get(aAlertName);
        if (alertCookie != null) {
            callObserver(aAlertName, "alertfinished", alertCookie);
            ALERT_COOKIES.remove(aAlertName);
        }

        removeObserver(aAlertName);

        int notificationID = aAlertName.hashCode();
        notificationClient.remove(notificationID);
    }

    public static void handleNotification(String aAction, String aAlertName, String aAlertCookie) {
        int notificationID = aAlertName.hashCode();

        if (GeckoApp.ACTION_ALERT_CALLBACK.equals(aAction)) {
            callObserver(aAlertName, "alertclickcallback", aAlertCookie);

            if (notificationClient.isOngoing(notificationID)) {
                // When clicked, keep the notification if it displays progress
                return;
            }
        }
        closeNotification(aAlertName);
    }

    @WrapElementForJNI(stubName = "GetDpiWrapper")
    public static int getDpi() {
        if (sDensityDpi == 0) {
            sDensityDpi = getContext().getResources().getDisplayMetrics().densityDpi;
        }

        return sDensityDpi;
    }

    @WrapElementForJNI
    public static float getDensity() {
        return getContext().getResources().getDisplayMetrics().density;
    }

    private static boolean isHighMemoryDevice() {
        return HardwareUtils.getMemSize() > HIGH_MEMORY_DEVICE_THRESHOLD_MB;
    }

    /**
     * Returns the colour depth of the default screen. This will either be
     * 24 or 16.
     */
    @WrapElementForJNI(stubName = "GetScreenDepthWrapper")
    public static synchronized int getScreenDepth() {
        if (sScreenDepth == 0) {
            sScreenDepth = 16;
            PixelFormat info = new PixelFormat();
            PixelFormat.getPixelFormatInfo(getGeckoInterface().getActivity().getWindowManager().getDefaultDisplay().getPixelFormat(), info);
            if (info.bitsPerPixel >= 24 && isHighMemoryDevice()) {
                sScreenDepth = 24;
            }
        }

        return sScreenDepth;
    }

    public static synchronized void setScreenDepthOverride(int aScreenDepth) {
        if (sScreenDepth != 0) {
            Log.e(LOGTAG, "Tried to override screen depth after it's already been set");
            return;
        }

        sScreenDepth = aScreenDepth;
    }

    @WrapElementForJNI
    public static void setFullScreen(boolean fullscreen) {
        if (getGeckoInterface() != null)
            getGeckoInterface().setFullScreen(fullscreen);
    }

    @WrapElementForJNI
    public static void performHapticFeedback(boolean aIsLongPress) {
        // Don't perform haptic feedback if a vibration is currently playing,
        // because the haptic feedback will nuke the vibration.
        if (!sVibrationMaybePlaying || System.nanoTime() >= sVibrationEndTime) {
            LayerView layerView = getLayerView();
            layerView.performHapticFeedback(aIsLongPress ?
                                            HapticFeedbackConstants.LONG_PRESS :
                                            HapticFeedbackConstants.VIRTUAL_KEY);
        }
    }

    private static Vibrator vibrator() {
        LayerView layerView = getLayerView();
        return (Vibrator) layerView.getContext().getSystemService(Context.VIBRATOR_SERVICE);
    }

    @WrapElementForJNI(stubName = "Vibrate1")
    public static void vibrate(long milliseconds) {
        sVibrationEndTime = System.nanoTime() + milliseconds * 1000000;
        sVibrationMaybePlaying = true;
        vibrator().vibrate(milliseconds);
    }

    @WrapElementForJNI(stubName = "VibrateA")
    public static void vibrate(long[] pattern, int repeat) {
        // If pattern.length is even, the last element in the pattern is a
        // meaningless delay, so don't include it in vibrationDuration.
        long vibrationDuration = 0;
        int iterLen = pattern.length - (pattern.length % 2 == 0 ? 1 : 0);
        for (int i = 0; i < iterLen; i++) {
          vibrationDuration += pattern[i];
        }

        sVibrationEndTime = System.nanoTime() + vibrationDuration * 1000000;
        sVibrationMaybePlaying = true;
        vibrator().vibrate(pattern, repeat);
    }

    @WrapElementForJNI
    public static void cancelVibrate() {
        sVibrationMaybePlaying = false;
        sVibrationEndTime = 0;
        vibrator().cancel();
    }

    @WrapElementForJNI
    public static void showInputMethodPicker() {
        InputMethodManager imm = (InputMethodManager)
            getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.showInputMethodPicker();
    }

    @WrapElementForJNI
    public static void setKeepScreenOn(final boolean on) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                // TODO
            }
        });
    }

    @WrapElementForJNI
    public static void notifyDefaultPrevented(final boolean defaultPrevented) {
        ThreadUtils.postToUiThread(new Runnable() {
            @Override
            public void run() {
                LayerView view = getLayerView();
                PanZoomController controller = (view == null ? null : view.getPanZoomController());
                if (controller != null) {
                    controller.notifyDefaultActionPrevented(defaultPrevented);
                }
            }
        });
    }

    @WrapElementForJNI
    public static boolean isNetworkLinkUp() {
        ConnectivityManager cm = (ConnectivityManager)
           getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        try {
            NetworkInfo info = cm.getActiveNetworkInfo();
            if (info == null || !info.isConnected())
                return false;
        } catch (SecurityException se) {
            return false;
        }
        return true;
    }

    @WrapElementForJNI
    public static boolean isNetworkLinkKnown() {
        ConnectivityManager cm = (ConnectivityManager)
            getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        try {
            if (cm.getActiveNetworkInfo() == null)
                return false;
        } catch (SecurityException se) {
            return false;
        }
        return true;
    }

    @WrapElementForJNI
    public static int networkLinkType() {
        ConnectivityManager cm = (ConnectivityManager)
            getContext().getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo info = cm.getActiveNetworkInfo();
        if (info == null) {
            return LINK_TYPE_UNKNOWN;
        }

        switch (info.getType()) {
            case ConnectivityManager.TYPE_ETHERNET:
                return LINK_TYPE_ETHERNET;
            case ConnectivityManager.TYPE_WIFI:
                return LINK_TYPE_WIFI;
            case ConnectivityManager.TYPE_WIMAX:
                return LINK_TYPE_WIMAX;
            case ConnectivityManager.TYPE_MOBILE:
                break; // We will handle sub-types after the switch.
            default:
                Log.w(LOGTAG, "Ignoring the current network type.");
                return LINK_TYPE_UNKNOWN;
        }

        TelephonyManager tm = (TelephonyManager)
            getContext().getSystemService(Context.TELEPHONY_SERVICE);
        if (tm == null) {
            Log.e(LOGTAG, "Telephony service does not exist");
            return LINK_TYPE_UNKNOWN;
        }

        switch (tm.getNetworkType()) {
            case TelephonyManager.NETWORK_TYPE_IDEN:
            case TelephonyManager.NETWORK_TYPE_CDMA:
            case TelephonyManager.NETWORK_TYPE_GPRS:
                return LINK_TYPE_2G;
            case TelephonyManager.NETWORK_TYPE_1xRTT:
            case TelephonyManager.NETWORK_TYPE_EDGE:
                return LINK_TYPE_2G; // 2.5G
            case TelephonyManager.NETWORK_TYPE_UMTS:
            case TelephonyManager.NETWORK_TYPE_EVDO_0:
                return LINK_TYPE_3G;
            case TelephonyManager.NETWORK_TYPE_HSPA:
            case TelephonyManager.NETWORK_TYPE_HSDPA:
            case TelephonyManager.NETWORK_TYPE_HSUPA:
            case TelephonyManager.NETWORK_TYPE_EVDO_A:
            case TelephonyManager.NETWORK_TYPE_EVDO_B:
            case TelephonyManager.NETWORK_TYPE_EHRPD:
                return LINK_TYPE_3G; // 3.5G
            case TelephonyManager.NETWORK_TYPE_HSPAP:
                return LINK_TYPE_3G; // 3.75G
            case TelephonyManager.NETWORK_TYPE_LTE:
                return LINK_TYPE_4G; // 3.9G
            case TelephonyManager.NETWORK_TYPE_UNKNOWN:
            default:
                Log.w(LOGTAG, "Connected to an unknown mobile network!");
                return LINK_TYPE_UNKNOWN;
        }
    }

    @WrapElementForJNI(stubName = "GetSystemColoursWrapper")
    public static int[] getSystemColors() {
        // attrsAppearance[] must correspond to AndroidSystemColors structure in android/AndroidBridge.h
        final int[] attrsAppearance = {
            android.R.attr.textColor,
            android.R.attr.textColorPrimary,
            android.R.attr.textColorPrimaryInverse,
            android.R.attr.textColorSecondary,
            android.R.attr.textColorSecondaryInverse,
            android.R.attr.textColorTertiary,
            android.R.attr.textColorTertiaryInverse,
            android.R.attr.textColorHighlight,
            android.R.attr.colorForeground,
            android.R.attr.colorBackground,
            android.R.attr.panelColorForeground,
            android.R.attr.panelColorBackground
        };

        int[] result = new int[attrsAppearance.length];

        final ContextThemeWrapper contextThemeWrapper =
            new ContextThemeWrapper(getContext(), android.R.style.TextAppearance);

        final TypedArray appearance = contextThemeWrapper.getTheme().obtainStyledAttributes(attrsAppearance);

        if (appearance != null) {
            for (int i = 0; i < appearance.getIndexCount(); i++) {
                int idx = appearance.getIndex(i);
                int color = appearance.getColor(idx, 0);
                result[idx] = color;
            }
            appearance.recycle();
        }

        return result;
    }

    @WrapElementForJNI
    public static void killAnyZombies() {
        GeckoProcessesVisitor visitor = new GeckoProcessesVisitor() {
            @Override
            public boolean callback(int pid) {
                if (pid != android.os.Process.myPid())
                    android.os.Process.killProcess(pid);
                return true;
            }
        };
            
        EnumerateGeckoProcesses(visitor);
    }

    public static boolean checkForGeckoProcs() {

        class GeckoPidCallback implements GeckoProcessesVisitor {
            public boolean otherPidExist = false;
            @Override
            public boolean callback(int pid) {
                if (pid != android.os.Process.myPid()) {
                    otherPidExist = true;
                    return false;
                }
                return true;
            }            
        }
        GeckoPidCallback visitor = new GeckoPidCallback();            
        EnumerateGeckoProcesses(visitor);
        return visitor.otherPidExist;
    }

    interface GeckoProcessesVisitor{
        boolean callback(int pid);
    }

    private static void EnumerateGeckoProcesses(GeckoProcessesVisitor visiter) {
        int pidColumn = -1;
        int userColumn = -1;

        try {
            // run ps and parse its output
            java.lang.Process ps = Runtime.getRuntime().exec("ps");
            BufferedReader in = new BufferedReader(new InputStreamReader(ps.getInputStream()),
                                                   2048);

            String headerOutput = in.readLine();

            // figure out the column offsets.  We only care about the pid and user fields
            StringTokenizer st = new StringTokenizer(headerOutput);
            
            int tokenSoFar = 0;
            while (st.hasMoreTokens()) {
                String next = st.nextToken();
                if (next.equalsIgnoreCase("PID"))
                    pidColumn = tokenSoFar;
                else if (next.equalsIgnoreCase("USER"))
                    userColumn = tokenSoFar;
                tokenSoFar++;
            }

            // alright, the rest are process entries.
            String psOutput = null;
            while ((psOutput = in.readLine()) != null) {
                String[] split = psOutput.split("\\s+");
                if (split.length <= pidColumn || split.length <= userColumn)
                    continue;
                int uid = android.os.Process.getUidForName(split[userColumn]);
                if (uid == android.os.Process.myUid() &&
                    !split[split.length - 1].equalsIgnoreCase("ps")) {
                    int pid = Integer.parseInt(split[pidColumn]);
                    boolean keepGoing = visiter.callback(pid);
                    if (keepGoing == false)
                        break;
                }
            }
            in.close();
        }
        catch (Exception e) {
            Log.w(LOGTAG, "Failed to enumerate Gecko processes.",  e);
        }
    }

    public static void waitForAnotherGeckoProc(){
        int countdown = 40;
        while (!checkForGeckoProcs() &&  --countdown > 0) {
            try {
                Thread.sleep(100);
            } catch (InterruptedException ie) {}
        }
    }
    public static String getAppNameByPID(int pid) {
        BufferedReader cmdlineReader = null;
        String path = "/proc/" + pid + "/cmdline";
        try {
            File cmdlineFile = new File(path);
            if (!cmdlineFile.exists())
                return "";
            cmdlineReader = new BufferedReader(new FileReader(cmdlineFile));
            return cmdlineReader.readLine().trim();
        } catch (Exception ex) {
            return "";
        } finally {
            if (null != cmdlineReader) {
                try {
                    cmdlineReader.close();
                } catch (Exception e) {}
            }
        }
    }

    public static void listOfOpenFiles() {
        int pidColumn = -1;
        int nameColumn = -1;

        try {
            String filter = GeckoProfile.get(getContext()).getDir().toString();
            Log.i(LOGTAG, "[OPENFILE] Filter: " + filter);

            // run lsof and parse its output
            java.lang.Process lsof = Runtime.getRuntime().exec("lsof");
            BufferedReader in = new BufferedReader(new InputStreamReader(lsof.getInputStream()), 2048);

            String headerOutput = in.readLine();
            StringTokenizer st = new StringTokenizer(headerOutput);
            int token = 0;
            while (st.hasMoreTokens()) {
                String next = st.nextToken();
                if (next.equalsIgnoreCase("PID"))
                    pidColumn = token;
                else if (next.equalsIgnoreCase("NAME"))
                    nameColumn = token;
                token++;
            }

            // alright, the rest are open file entries.
            Map<Integer, String> pidNameMap = new TreeMap<Integer, String>();
            String output = null;
            while ((output = in.readLine()) != null) {
                String[] split = output.split("\\s+");
                if (split.length <= pidColumn || split.length <= nameColumn)
                    continue;
                Integer pid = new Integer(split[pidColumn]);
                String name = pidNameMap.get(pid);
                if (name == null) {
                    name = getAppNameByPID(pid.intValue());
                    pidNameMap.put(pid, name);
                }
                String file = split[nameColumn];
                if (!TextUtils.isEmpty(name) && !TextUtils.isEmpty(file) && file.startsWith(filter))
                    Log.i(LOGTAG, "[OPENFILE] " + name + "(" + split[pidColumn] + ") : " + file);
            }
            in.close();
        } catch (Exception e) { }
    }

    @WrapElementForJNI
    public static void scanMedia(String aFile, String aMimeType) {
        // If the platform didn't give us a mimetype, try to guess one from the filename
        if (TextUtils.isEmpty(aMimeType)) {
            int extPosition = aFile.lastIndexOf(".");
            if (extPosition > 0 && extPosition < aFile.length() - 1) {
                aMimeType = getMimeTypeFromExtension(aFile.substring(extPosition+1));
            }
        }

        Context context = getContext();
        GeckoMediaScannerClient.startScan(context, aFile, aMimeType);
    }

    @WrapElementForJNI(stubName = "GetIconForExtensionWrapper")
    public static byte[] getIconForExtension(String aExt, int iconSize) {
        try {
            if (iconSize <= 0)
                iconSize = 16;

            if (aExt != null && aExt.length() > 1 && aExt.charAt(0) == '.')
                aExt = aExt.substring(1);

            PackageManager pm = getContext().getPackageManager();
            Drawable icon = getDrawableForExtension(pm, aExt);
            if (icon == null) {
                // Use a generic icon
                icon = pm.getDefaultActivityIcon();
            }

            Bitmap bitmap = ((BitmapDrawable)icon).getBitmap();
            if (bitmap.getWidth() != iconSize || bitmap.getHeight() != iconSize)
                bitmap = Bitmap.createScaledBitmap(bitmap, iconSize, iconSize, true);

            ByteBuffer buf = ByteBuffer.allocate(iconSize * iconSize * 4);
            bitmap.copyPixelsToBuffer(buf);

            return buf.array();
        }
        catch (Exception e) {
            Log.w(LOGTAG, "getIconForExtension failed.",  e);
            return null;
        }
    }

    private static String getMimeTypeFromExtension(String ext) {
        final MimeTypeMap mtm = MimeTypeMap.getSingleton();
        return mtm.getMimeTypeFromExtension(ext);
    }
    
    private static Drawable getDrawableForExtension(PackageManager pm, String aExt) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        final String mimeType = getMimeTypeFromExtension(aExt);
        if (mimeType != null && mimeType.length() > 0)
            intent.setType(mimeType);
        else
            return null;

        List<ResolveInfo> list = pm.queryIntentActivities(intent, 0);
        if (list.size() == 0)
            return null;

        ResolveInfo resolveInfo = list.get(0);

        if (resolveInfo == null)
            return null;

        ActivityInfo activityInfo = resolveInfo.activityInfo;

        return activityInfo.loadIcon(pm);
    }

    @WrapElementForJNI
    public static boolean getShowPasswordSetting() {
        try {
            int showPassword =
                Settings.System.getInt(getContext().getContentResolver(),
                                       Settings.System.TEXT_SHOW_PASSWORD, 1);
            return (showPassword > 0);
        }
        catch (Exception e) {
            return true;
        }
    }

    @WrapElementForJNI(stubName = "AddPluginViewWrapper")
    public static void addPluginView(View view,
                                     float x, float y,
                                     float w, float h,
                                     boolean isFullScreen) {
        if (getGeckoInterface() != null)
             getGeckoInterface().addPluginView(view, new RectF(x, y, x + w, y + h), isFullScreen);
    }

    @WrapElementForJNI
    public static void removePluginView(View view, boolean isFullScreen) {
        if (getGeckoInterface() != null)
            getGeckoInterface().removePluginView(view, isFullScreen);
    }

    /**
     * A plugin that wish to be loaded in the WebView must provide this permission
     * in their AndroidManifest.xml.
     */
    public static final String PLUGIN_ACTION = "android.webkit.PLUGIN";
    public static final String PLUGIN_PERMISSION = "android.webkit.permission.PLUGIN";

    private static final String PLUGIN_SYSTEM_LIB = "/system/lib/plugins/";

    private static final String PLUGIN_TYPE = "type";
    private static final String TYPE_NATIVE = "native";
    static public ArrayList<PackageInfo> mPackageInfoCache = new ArrayList<PackageInfo>();

    // Returns null if plugins are blocked on the device.
    static String[] getPluginDirectories() {

        // An awful hack to detect Tegra devices. Easiest way to do it without spinning up a EGL context.
        boolean isTegra = (new File("/system/lib/hw/gralloc.tegra.so")).exists() ||
                          (new File("/system/lib/hw/gralloc.tegra3.so")).exists();
        if (isTegra) {
            // disable Flash on Tegra ICS with CM9 and other custom firmware (bug 736421)
            File vfile = new File("/proc/version");
            FileReader vreader = null;
            try {
                if (vfile.canRead()) {
                    vreader = new FileReader(vfile);
                    String version = new BufferedReader(vreader).readLine();
                    if (version.indexOf("CM9") != -1 ||
                        version.indexOf("cyanogen") != -1 ||
                        version.indexOf("Nova") != -1)
                    {
                        Log.w(LOGTAG, "Blocking plugins because of Tegra 2 + unofficial ICS bug (bug 736421)");
                        return null;
                    }
                }
            } catch (IOException ex) {
                // nothing
            } finally {
                try {
                    if (vreader != null) {
                        vreader.close();
                    }
                } catch (IOException ex) {
                    // nothing
                }
            }

            // disable on KitKat (bug 957694)
            if (Build.VERSION.SDK_INT >= 19) {
                Log.w(LOGTAG, "Blocking plugins because of Tegra (bug 957694)");
                return null;
            }
        }

        ArrayList<String> directories = new ArrayList<String>();
        PackageManager pm = getContext().getPackageManager();
        List<ResolveInfo> plugins = pm.queryIntentServices(new Intent(PLUGIN_ACTION),
                PackageManager.GET_SERVICES | PackageManager.GET_META_DATA);

        synchronized(mPackageInfoCache) {

            // clear the list of existing packageInfo objects
            mPackageInfoCache.clear();


            for (ResolveInfo info : plugins) {

                // retrieve the plugin's service information
                ServiceInfo serviceInfo = info.serviceInfo;
                if (serviceInfo == null) {
                    Log.w(LOGTAG, "Ignoring bad plugin.");
                    continue;
                }

                // Blacklist HTC's flash lite.
                // See bug #704516 - We're not quite sure what Flash Lite does,
                // but loading it causes Flash to give errors and fail to draw.
                if (serviceInfo.packageName.equals("com.htc.flashliteplugin")) {
                    Log.w(LOGTAG, "Skipping HTC's flash lite plugin");
                    continue;
                }


                // Retrieve information from the plugin's manifest.
                PackageInfo pkgInfo;
                try {
                    pkgInfo = pm.getPackageInfo(serviceInfo.packageName,
                                    PackageManager.GET_PERMISSIONS
                                    | PackageManager.GET_SIGNATURES);
                } catch (Exception e) {
                    Log.w(LOGTAG, "Can't find plugin: " + serviceInfo.packageName);
                    continue;
                }

                if (pkgInfo == null) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Could not load package information.");
                    continue;
                }

                /*
                 * find the location of the plugin's shared library. The default
                 * is to assume the app is either a user installed app or an
                 * updated system app. In both of these cases the library is
                 * stored in the app's data directory.
                 */
                String directory = pkgInfo.applicationInfo.dataDir + "/lib";
                final int appFlags = pkgInfo.applicationInfo.flags;
                final int updatedSystemFlags = ApplicationInfo.FLAG_SYSTEM |
                                               ApplicationInfo.FLAG_UPDATED_SYSTEM_APP;

                // preloaded system app with no user updates
                if ((appFlags & updatedSystemFlags) == ApplicationInfo.FLAG_SYSTEM) {
                    directory = PLUGIN_SYSTEM_LIB + pkgInfo.packageName;
                }

                // check if the plugin has the required permissions
                String permissions[] = pkgInfo.requestedPermissions;
                if (permissions == null) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Does not have required permission.");
                    continue;
                }
                boolean permissionOk = false;
                for (String permit : permissions) {
                    if (PLUGIN_PERMISSION.equals(permit)) {
                        permissionOk = true;
                        break;
                    }
                }
                if (!permissionOk) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Does not have required permission (2).");
                    continue;
                }

                // check to ensure the plugin is properly signed
                Signature signatures[] = pkgInfo.signatures;
                if (signatures == null) {
                    Log.w(LOGTAG, "Not loading plugin: " + serviceInfo.packageName + ". Not signed.");
                    continue;
                }

                // determine the type of plugin from the manifest
                if (serviceInfo.metaData == null) {
                    Log.e(LOGTAG, "The plugin '" + serviceInfo.name + "' has no defined type.");
                    continue;
                }

                String pluginType = serviceInfo.metaData.getString(PLUGIN_TYPE);
                if (!TYPE_NATIVE.equals(pluginType)) {
                    Log.e(LOGTAG, "Unrecognized plugin type: " + pluginType);
                    continue;
                }

                try {
                    Class<?> cls = getPluginClass(serviceInfo.packageName, serviceInfo.name);

                    //TODO implement any requirements of the plugin class here!
                    boolean classFound = true;

                    if (!classFound) {
                        Log.e(LOGTAG, "The plugin's class' " + serviceInfo.name + "' does not extend the appropriate class.");
                        continue;
                    }

                } catch (NameNotFoundException e) {
                    Log.e(LOGTAG, "Can't find plugin: " + serviceInfo.packageName);
                    continue;
                } catch (ClassNotFoundException e) {
                    Log.e(LOGTAG, "Can't find plugin's class: " + serviceInfo.name);
                    continue;
                }

                // if all checks have passed then make the plugin available
                mPackageInfoCache.add(pkgInfo);
                directories.add(directory);
            }
        }

        return directories.toArray(new String[directories.size()]);
    }

    static String getPluginPackage(String pluginLib) {

        if (pluginLib == null || pluginLib.length() == 0) {
            return null;
        }

        synchronized(mPackageInfoCache) {
            for (PackageInfo pkgInfo : mPackageInfoCache) {
                if (pluginLib.contains(pkgInfo.packageName)) {
                    return pkgInfo.packageName;
                }
            }
        }

        return null;
    }

    static Class<?> getPluginClass(String packageName, String className)
            throws NameNotFoundException, ClassNotFoundException {
        Context pluginContext = getContext().createPackageContext(packageName,
                Context.CONTEXT_INCLUDE_CODE |
                Context.CONTEXT_IGNORE_SECURITY);
        ClassLoader pluginCL = pluginContext.getClassLoader();
        return pluginCL.loadClass(className);
    }

    @WrapElementForJNI(allowMultithread = true)
    public static Class<?> loadPluginClass(String className, String libName) {
        if (getGeckoInterface() == null)
            return null;
        try {
            final String packageName = getPluginPackage(libName);
            final int contextFlags = Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY;
            final Context pluginContext = getContext().createPackageContext(packageName, contextFlags);
            return pluginContext.getClassLoader().loadClass(className);
        } catch (java.lang.ClassNotFoundException cnfe) {
            Log.w(LOGTAG, "Couldn't find plugin class " + className, cnfe);
            return null;
        } catch (android.content.pm.PackageManager.NameNotFoundException nnfe) {
            Log.w(LOGTAG, "Couldn't find package.", nnfe);
            return null;
        }
    }

    private static ContextGetter sContextGetter;

    @WrapElementForJNI(allowMultithread = true)
    public static Context getContext() {
        return sContextGetter.getContext();
    }

    public static void setContextGetter(ContextGetter cg) {
        sContextGetter = cg;
    }

    public static SharedPreferences getSharedPreferences() {
        if (sContextGetter == null) {
            throw new IllegalStateException("No ContextGetter; cannot fetch prefs.");
        }
        return sContextGetter.getSharedPreferences();
    }

    public interface AppStateListener {
        public void onPause();
        public void onResume();
        public void onOrientationChanged();
    }

    public interface GeckoInterface {
        public GeckoProfile getProfile();
        public PromptService getPromptService();
        public Activity getActivity();
        public String getDefaultUAString();
        public LocationListener getLocationListener();
        public SensorEventListener getSensorEventListener();
        public void doRestart();
        public void setFullScreen(boolean fullscreen);
        public void addPluginView(View view, final RectF rect, final boolean isFullScreen);
        public void removePluginView(final View view, final boolean isFullScreen);
        public void enableCameraView();
        public void disableCameraView();
        public void addAppStateListener(AppStateListener listener);
        public void removeAppStateListener(AppStateListener listener);
        public View getCameraView();
        public void notifyWakeLockChanged(String topic, String state);
        public FormAssistPopup getFormAssistPopup();
        public boolean areTabsShown();
        public AbsoluteLayout getPluginContainer();
        public void notifyCheckUpdateResult(String result);
        public boolean hasTabsSideBar();
        public void invalidateOptionsMenu();
    };

    private static GeckoInterface sGeckoInterface;

    public static GeckoInterface getGeckoInterface() {
        return sGeckoInterface;
    }

    public static void setGeckoInterface(GeckoInterface aGeckoInterface) {
        sGeckoInterface = aGeckoInterface;
    }

    public static android.hardware.Camera sCamera = null;

    static native void cameraCallbackBridge(byte[] data);

    static int kPreferedFps = 25;
    static byte[] sCameraBuffer = null;


    @WrapElementForJNI(stubName = "InitCameraWrapper")
    static int[] initCamera(String aContentType, int aCamera, int aWidth, int aHeight) {
        ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        if (getGeckoInterface() != null)
                            getGeckoInterface().enableCameraView();
                    } catch (Exception e) {}
                }
            });

        // [0] = 0|1 (failure/success)
        // [1] = width
        // [2] = height
        // [3] = fps
        int[] result = new int[4];
        result[0] = 0;

        if (Build.VERSION.SDK_INT >= 9) {
            if (android.hardware.Camera.getNumberOfCameras() == 0)
                return result;
        }

        try {
            // no front/back camera before API level 9
            if (Build.VERSION.SDK_INT >= 9)
                sCamera = android.hardware.Camera.open(aCamera);
            else
                sCamera = android.hardware.Camera.open();

            android.hardware.Camera.Parameters params = sCamera.getParameters();
            params.setPreviewFormat(ImageFormat.NV21);

            // use the preview fps closest to 25 fps.
            int fpsDelta = 1000;
            try {
                Iterator<Integer> it = params.getSupportedPreviewFrameRates().iterator();
                while (it.hasNext()) {
                    int nFps = it.next();
                    if (Math.abs(nFps - kPreferedFps) < fpsDelta) {
                        fpsDelta = Math.abs(nFps - kPreferedFps);
                        params.setPreviewFrameRate(nFps);
                    }
                }
            } catch(Exception e) {
                params.setPreviewFrameRate(kPreferedFps);
            }

            // set up the closest preview size available
            Iterator<android.hardware.Camera.Size> sit = params.getSupportedPreviewSizes().iterator();
            int sizeDelta = 10000000;
            int bufferSize = 0;
            while (sit.hasNext()) {
                android.hardware.Camera.Size size = sit.next();
                if (Math.abs(size.width * size.height - aWidth * aHeight) < sizeDelta) {
                    sizeDelta = Math.abs(size.width * size.height - aWidth * aHeight);
                    params.setPreviewSize(size.width, size.height);
                    bufferSize = size.width * size.height;
                }
            }

            try {
                if (getGeckoInterface() != null) {
                    View cameraView = getGeckoInterface().getCameraView();
                    if (cameraView instanceof SurfaceView) {
                        sCamera.setPreviewDisplay(((SurfaceView)cameraView).getHolder());
                    } else if (cameraView instanceof TextureView) {
                        sCamera.setPreviewTexture(((TextureView)cameraView).getSurfaceTexture());
                    }
                }
            } catch(IOException e) {
                Log.w(LOGTAG, "Error setPreviewXXX:", e);
            } catch(RuntimeException e) {
                Log.w(LOGTAG, "Error setPreviewXXX:", e);
            }

            sCamera.setParameters(params);
            sCameraBuffer = new byte[(bufferSize * 12) / 8];
            sCamera.addCallbackBuffer(sCameraBuffer);
            sCamera.setPreviewCallbackWithBuffer(new android.hardware.Camera.PreviewCallback() {
                @Override
                public void onPreviewFrame(byte[] data, android.hardware.Camera camera) {
                    cameraCallbackBridge(data);
                    if (sCamera != null)
                        sCamera.addCallbackBuffer(sCameraBuffer);
                }
            });
            sCamera.startPreview();
            params = sCamera.getParameters();
            result[0] = 1;
            result[1] = params.getPreviewSize().width;
            result[2] = params.getPreviewSize().height;
            result[3] = params.getPreviewFrameRate();
        } catch(RuntimeException e) {
            Log.w(LOGTAG, "initCamera RuntimeException.", e);
            result[0] = result[1] = result[2] = result[3] = 0;
        }
        return result;
    }

    @WrapElementForJNI
    static synchronized void closeCamera() {
        ThreadUtils.postToUiThread(new Runnable() {
                @Override
                public void run() {
                    try {
                        if (getGeckoInterface() != null)
                            getGeckoInterface().disableCameraView();
                    } catch (Exception e) {}
                }
            });
        if (sCamera != null) {
            sCamera.stopPreview();
            sCamera.release();
            sCamera = null;
            sCameraBuffer = null;
        }
    }

    /*
     * Battery API related methods.
     */
    @WrapElementForJNI
    public static void enableBatteryNotifications() {
        GeckoBatteryManager.enableNotifications();
    }

    @WrapElementForJNI(stubName = "HandleGeckoMessageWrapper")
    public static void handleGeckoMessage(final NativeJSContainer message) {
        EventDispatcher.getInstance().dispatchEvent(message);
        message.dispose();
    }

    @WrapElementForJNI
    public static void disableBatteryNotifications() {
        GeckoBatteryManager.disableNotifications();
    }

    @WrapElementForJNI(stubName = "GetCurrentBatteryInformationWrapper")
    public static double[] getCurrentBatteryInformation() {
        return GeckoBatteryManager.getCurrentInformation();
    }

    @WrapElementForJNI(stubName = "CheckURIVisited")
    static void checkUriVisited(String uri) {
        GlobalHistory.getInstance().checkUriVisited(uri);
    }

    @WrapElementForJNI(stubName = "MarkURIVisited")
    static void markUriVisited(final String uri) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                GlobalHistory.getInstance().add(uri);
            }
        });
    }

    @WrapElementForJNI(stubName = "SetURITitle")
    static void setUriTitle(final String uri, final String title) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                GlobalHistory.getInstance().update(uri, title);
            }
        });
    }

    @WrapElementForJNI
    static void hideProgressDialog() {
        // unused stub
    }

    /*
     * WebSMS related methods.
     */
    @WrapElementForJNI(stubName = "SendMessageWrapper")
    public static void sendMessage(String aNumber, String aMessage, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().send(aNumber, aMessage, aRequestId);
    }

    @WrapElementForJNI(stubName = "GetMessageWrapper")
    public static void getMessage(int aMessageId, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().getMessage(aMessageId, aRequestId);
    }

    @WrapElementForJNI(stubName = "DeleteMessageWrapper")
    public static void deleteMessage(int aMessageId, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().deleteMessage(aMessageId, aRequestId);
    }

    @WrapElementForJNI(stubName = "CreateMessageListWrapper")
    public static void createMessageList(long aStartDate, long aEndDate, String[] aNumbers, int aNumbersCount, int aDeliveryState, boolean aReverse, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().createMessageList(aStartDate, aEndDate, aNumbers, aNumbersCount, aDeliveryState, aReverse, aRequestId);
    }

    @WrapElementForJNI(stubName = "GetNextMessageInListWrapper")
    public static void getNextMessageInList(int aListId, int aRequestId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().getNextMessageInList(aListId, aRequestId);
    }

    @WrapElementForJNI
    public static void clearMessageList(int aListId) {
        if (SmsManager.getInstance() == null) {
            return;
        }

        SmsManager.getInstance().clearMessageList(aListId);
    }

    /* Called by JNI from AndroidBridge, and by reflection from tests/BaseTest.java.in */
    @WrapElementForJNI
    @RobocopTarget
    public static boolean isTablet() {
        return HardwareUtils.isTablet();
    }

    public static void viewSizeChanged() {
        LayerView v = getLayerView();
        if (v != null && v.isIMEEnabled()) {
            sendEventToGecko(GeckoEvent.createBroadcastEvent(
                    "ScrollTo:FocusedInput", ""));
        }
    }

    @WrapElementForJNI(stubName = "GetCurrentNetworkInformationWrapper")
    public static double[] getCurrentNetworkInformation() {
        return GeckoNetworkManager.getInstance().getCurrentInformation();
    }

    @WrapElementForJNI
    public static void enableNetworkNotifications() {
        GeckoNetworkManager.getInstance().enableNotifications();
    }

    @WrapElementForJNI
    public static void disableNetworkNotifications() {
        GeckoNetworkManager.getInstance().disableNotifications();
    }

    // values taken from android's Base64
    public static final int BASE64_DEFAULT = 0;
    public static final int BASE64_URL_SAFE = 8;

    /**
     * taken from http://www.source-code.biz/base64coder/java/Base64Coder.java.txt and modified (MIT License)
     */
    // Mapping table from 6-bit nibbles to Base64 characters.
    private static final byte[] map1 = new byte[64];
    private static final byte[] map1_urlsafe;
    static {
      int i=0;
      for (byte c='A'; c<='Z'; c++) map1[i++] = c;
      for (byte c='a'; c<='z'; c++) map1[i++] = c;
      for (byte c='0'; c<='9'; c++) map1[i++] = c;
      map1[i++] = '+'; map1[i++] = '/';
      map1_urlsafe = map1.clone();
      map1_urlsafe[62] = '-'; map1_urlsafe[63] = '_'; 
    }

    // Mapping table from Base64 characters to 6-bit nibbles.
    private static final byte[] map2 = new byte[128];
    static {
        for (int i=0; i<map2.length; i++) map2[i] = -1;
        for (int i=0; i<64; i++) map2[map1[i]] = (byte)i;
        map2['-'] = (byte)62; map2['_'] = (byte)63;
    }

    final static byte EQUALS_ASCII = (byte) '=';

    /**
     * Encodes a byte array into Base64 format.
     * No blanks or line breaks are inserted in the output.
     * @param in    An array containing the data bytes to be encoded.
     * @return      A character array containing the Base64 encoded data.
     */
    public static byte[] encodeBase64(byte[] in, int flags) {
        if (Build.VERSION.SDK_INT >=Build.VERSION_CODES.FROYO)
            return Base64.encode(in, flags | Base64.NO_WRAP);
        int oDataLen = (in.length*4+2)/3;       // output length without padding
        int oLen = ((in.length+2)/3)*4;         // output length including padding
        byte[] out = new byte[oLen];
        int ip = 0;
        int iEnd = in.length;
        int op = 0;
        byte[] toMap = ((flags & BASE64_URL_SAFE) == 0 ? map1 : map1_urlsafe);
        while (ip < iEnd) {
            int i0 = in[ip++] & 0xff;
            int i1 = ip < iEnd ? in[ip++] & 0xff : 0;
            int i2 = ip < iEnd ? in[ip++] & 0xff : 0;
            int o0 = i0 >>> 2;
            int o1 = ((i0 &   3) << 4) | (i1 >>> 4);
            int o2 = ((i1 & 0xf) << 2) | (i2 >>> 6);
            int o3 = i2 & 0x3F;
            out[op++] = toMap[o0];
            out[op++] = toMap[o1];
            out[op] = op < oDataLen ? toMap[o2] : EQUALS_ASCII; op++;
            out[op] = op < oDataLen ? toMap[o3] : EQUALS_ASCII; op++;
        }
        return out; 
    }

    /**
     * Decodes a byte array from Base64 format.
     * No blanks or line breaks are allowed within the Base64 encoded input data.
     * @param in    A character array containing the Base64 encoded data.
     * @param iOff  Offset of the first character in <code>in</code> to be processed.
     * @param iLen  Number of characters to process in <code>in</code>, starting at <code>iOff</code>.
     * @return      An array containing the decoded data bytes.
     * @throws      IllegalArgumentException If the input is not valid Base64 encoded data.
     */
    public static byte[] decodeBase64(byte[] in, int flags) {
        if (Build.VERSION.SDK_INT >=Build.VERSION_CODES.FROYO)
            return Base64.decode(in, flags);
        int iOff = 0;
        int iLen = in.length;
        if (iLen%4 != 0) throw new IllegalArgumentException ("Length of Base64 encoded input string is not a multiple of 4.");
        while (iLen > 0 && in[iOff+iLen-1] == '=') iLen--;
        int oLen = (iLen*3) / 4;
        byte[] out = new byte[oLen];
        int ip = iOff;
        int iEnd = iOff + iLen;
        int op = 0;
        while (ip < iEnd) {
            int i0 = in[ip++];
            int i1 = in[ip++];
            int i2 = ip < iEnd ? in[ip++] : 'A';
            int i3 = ip < iEnd ? in[ip++] : 'A';
            if (i0 > 127 || i1 > 127 || i2 > 127 || i3 > 127)
                throw new IllegalArgumentException ("Illegal character in Base64 encoded data.");
            int b0 = map2[i0];
            int b1 = map2[i1];
            int b2 = map2[i2];
            int b3 = map2[i3];
            if (b0 < 0 || b1 < 0 || b2 < 0 || b3 < 0)
                throw new IllegalArgumentException ("Illegal character in Base64 encoded data.");
            int o0 = ( b0       <<2) | (b1>>>4);
            int o1 = ((b1 & 0xf)<<4) | (b2>>>2);
            int o2 = ((b2 &   3)<<6) |  b3;
            out[op++] = (byte)o0;
            if (op<oLen) out[op++] = (byte)o1;
            if (op<oLen) out[op++] = (byte)o2; }
        return out; 
    }

    public static byte[] decodeBase64(String s, int flags) {
        return decodeBase64(s.getBytes(), flags);
    }

    @WrapElementForJNI(stubName = "GetScreenOrientationWrapper")
    public static short getScreenOrientation() {
        return GeckoScreenOrientation.getInstance().getScreenOrientation().value;
    }

    @WrapElementForJNI
    public static void enableScreenOrientationNotifications() {
        GeckoScreenOrientation.getInstance().enableNotifications();
    }

    @WrapElementForJNI
    public static void disableScreenOrientationNotifications() {
        GeckoScreenOrientation.getInstance().disableNotifications();
    }

    @WrapElementForJNI
    public static void lockScreenOrientation(int aOrientation) {
        GeckoScreenOrientation.getInstance().lock(aOrientation);
    }

    @WrapElementForJNI
    public static void unlockScreenOrientation() {
        GeckoScreenOrientation.getInstance().unlock();
    }

    @WrapElementForJNI
    public static boolean pumpMessageLoop() {
        Handler geckoHandler = ThreadUtils.sGeckoHandler;
        Message msg = getNextMessageFromQueue(ThreadUtils.sGeckoQueue);

        if (msg == null)
            return false;
        if (msg.obj == geckoHandler && msg.getTarget() == geckoHandler) {
            // Our "queue is empty" message; see runGecko()
            msg.recycle();
            return false;
        }
        if (msg.getTarget() == null) 
            Looper.myLooper().quit();
        else
            msg.getTarget().dispatchMessage(msg);
        msg.recycle();
        return true;
    }

    @WrapElementForJNI
    public static void notifyWakeLockChanged(String topic, String state) {
        if (getGeckoInterface() != null)
            getGeckoInterface().notifyWakeLockChanged(topic, state);
    }

    @WrapElementForJNI
    public static void registerSurfaceTextureFrameListener(Object surfaceTexture, final int id) {
        ((SurfaceTexture)surfaceTexture).setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
            @Override
            public void onFrameAvailable(SurfaceTexture surfaceTexture) {
                GeckoAppShell.onSurfaceTextureFrameAvailable(surfaceTexture, id);
            }
        });
    }

    @WrapElementForJNI(allowMultithread = true)
    public static void unregisterSurfaceTextureFrameListener(Object surfaceTexture) {
        ((SurfaceTexture)surfaceTexture).setOnFrameAvailableListener(null);
    }

    @WrapElementForJNI
    public static boolean unlockProfile() {
        // Try to kill any zombie Fennec's that might be running
        GeckoAppShell.killAnyZombies();

        // Then force unlock this profile
        if (getGeckoInterface() != null) {
            GeckoProfile profile = getGeckoInterface().getProfile();
            File lock = profile.getFile(".parentlock");
            return lock.exists() && lock.delete();
        }
        return false;
    }

    @WrapElementForJNI(stubName = "GetProxyForURIWrapper")
    public static String getProxyForURI(String spec, String scheme, String host, int port) {
        final ProxySelector ps = new ProxySelector();

        Proxy proxy = ps.select(scheme, host);
        if (Proxy.NO_PROXY.equals(proxy)) {
            return "DIRECT";
        }
        
        switch (proxy.type()) {
            case HTTP:
                return "PROXY " + proxy.address().toString();
            case SOCKS:
                return "SOCKS " + proxy.address().toString();
        }

        return "DIRECT";
    }

    /* Downloads the uri pointed to by a share intent, and alters the intent to point to the locally stored file.
     */
    public static void downloadImageForIntent(final Intent intent) {
        final String src = intent.getStringExtra(Intent.EXTRA_TEXT);
        final File dir = GeckoApp.getTempDirectory();

        if (dir == null) {
            showImageShareFailureToast();
            return;
        }

        GeckoApp.deleteTempFiles();

        String type = intent.getType();
        OutputStream os = null;
        try {
            // Create a temporary file for the image
            if (src.startsWith("data:")) {
                final int dataStart = src.indexOf(",");

                String extension = MimeTypeMap.getSingleton().getExtensionFromMimeType(type);

                // If we weren't given an explicit mimetype, try to dig one out of the data uri.
                if (TextUtils.isEmpty(extension) && dataStart > 5) {
                    type = src.substring(5, dataStart).replace(";base64", "");
                    extension = MimeTypeMap.getSingleton().getExtensionFromMimeType(type);
                }

                final File imageFile = File.createTempFile("image", "." + extension, dir);
                os = new FileOutputStream(imageFile);

                byte[] buf = Base64.decode(src.substring(dataStart + 1), Base64.DEFAULT);
                os.write(buf);

                // Only alter the intent when we're sure everything has worked
                intent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(imageFile));
            } else {
                InputStream is = null;
                try {
                    final byte[] buf = new byte[2048];
                    final URL url = new URL(src);
                    final String filename = URLUtil.guessFileName(src, null, type);
                    is = url.openStream();

                    final File imageFile = new File(dir, filename);
                    os = new FileOutputStream(imageFile);

                    int length;
                    while ((length = is.read(buf)) != -1) {
                        os.write(buf, 0, length);
                    }

                    // Only alter the intent when we're sure everything has worked
                    intent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(imageFile));
                } finally {
                    safeStreamClose(is);
                }
            }
        } catch(IOException ex) {
            // If something went wrong, we'll just leave the intent un-changed
        } finally {
            safeStreamClose(os);
        }
    }

    // Don't fail silently, tell the user that we weren't able to share the image
    private static final void showImageShareFailureToast() {
        Toast toast = Toast.makeText(getContext(),
                                     getContext().getResources().getString(R.string.share_image_failed),
                                     Toast.LENGTH_SHORT);
        toast.show();
    }

    @WrapElementForJNI(allowMultithread = true)
    static InputStream createInputStream(URLConnection connection) throws IOException {
        return connection.getInputStream();
    }

    @WrapElementForJNI(allowMultithread = true, narrowChars = true)
    static URLConnection getConnection(String url) {
        try {
            String spec;
            if (url.startsWith("android://")) {
                spec = url.substring(10);
            } else {
                spec = url.substring(8);
            }

            // if the colon got stripped, put it back
            int colon = spec.indexOf(':');
            if (colon == -1 || colon > spec.indexOf('/')) {
                spec = spec.replaceFirst("/", ":/");
            }
        } catch(Exception ex) {
            return null;
        }
        return null;
    }

    @WrapElementForJNI(allowMultithread = true, narrowChars = true)
    static String connectionGetMimeType(URLConnection connection) {
        return connection.getContentType();
    }

}
