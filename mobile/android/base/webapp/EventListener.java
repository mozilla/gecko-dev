/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.webapp;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.ActivityHandlerHelper;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.GeckoAppShell;
import org.mozilla.gecko.GeckoProfile;
import org.mozilla.gecko.util.ActivityResultHandler;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.NativeEventListener;
import org.mozilla.gecko.util.NativeJSObject;
import org.mozilla.gecko.util.ThreadUtils;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

public class EventListener implements NativeEventListener  {

    private static final String LOGTAG = "GeckoWebappEventListener";

    public void registerEvents() {
        EventDispatcher.getInstance().registerGeckoThreadListener(this,
            "Webapps:Preinstall",
            "Webapps:InstallApk",
            "Webapps:UninstallApk",
            "Webapps:Postinstall",
            "Webapps:Open",
            "Webapps:Uninstall",
            "Webapps:GetApkVersions");
    }

    public void unregisterEvents() {
        EventDispatcher.getInstance().unregisterGeckoThreadListener(this,
            "Webapps:Preinstall",
            "Webapps:InstallApk",
            "Webapps:UninstallApk",
            "Webapps:Postinstall",
            "Webapps:Open",
            "Webapps:Uninstall",
            "Webapps:GetApkVersions");
    }

    @Override
    public void handleMessage(String event, NativeJSObject message, EventCallback callback) {
        try {
            if (event.equals("Webapps:InstallApk")) {
                installApk(GeckoAppShell.getGeckoInterface().getActivity(), message, callback);
            } else if (event.equals("Webapps:UninstallApk")) {
                uninstallApk(GeckoAppShell.getGeckoInterface().getActivity(), message);
            } else if (event.equals("Webapps:Postinstall")) {
                postInstallWebapp(message.getString("apkPackageName"), message.getString("origin"));
            } else if (event.equals("Webapps:Open")) {
                Intent intent = GeckoAppShell.getWebappIntent(message.getString("manifestURL"),
                                                              message.getString("origin"),
                                                              "", null);
                if (intent == null) {
                    return;
                }
                GeckoAppShell.getGeckoInterface().getActivity().startActivity(intent);
            } else if (event.equals("Webapps:GetApkVersions")) {
                JSONObject obj = new JSONObject();
                obj.put("versions", getApkVersions(GeckoAppShell.getGeckoInterface().getActivity(),
                                                   message.getStringArray("packageNames")));
                callback.sendSuccess(obj);
            }
        } catch (Exception e) {
            Log.e(LOGTAG, "Exception handling message \"" + event + "\":", e);
        }
    }

    public static void postInstallWebapp(String aPackageName, String aOrigin) {
        Allocator allocator = Allocator.getInstance(GeckoAppShell.getContext());
        int index = allocator.findOrAllocatePackage(aPackageName);
        allocator.putOrigin(index, aOrigin);
    }

    public static void uninstallWebapp(final String packageName) {
        // On uninstall, we need to do a couple of things:
        //   1. nuke the running app process.
        //   2. nuke the profile that was assigned to that webapp
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                int index = Allocator.getInstance(GeckoAppShell.getContext()).releaseIndexForApp(packageName);

                // if -1, nothing to do; we didn't think it was installed anyway
                if (index == -1)
                    return;

                killWebappSlot(GeckoAppShell.getContext(), index);

                // then nuke the profile
                GeckoProfile.removeProfile(GeckoAppShell.getContext(), "webapp" + index);
            }
        });
    }

    /**
     * Used in both uninstall and swiping away from the recent task list.
     *
     * @param context
     * @param slot
     */
    public static void killWebappSlot(Context context, int slot) {
        // kill the app if it's running
        String targetProcessName = context.getPackageName();
        targetProcessName = targetProcessName + ":" + targetProcessName + ".Webapp" + slot;

        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningAppProcessInfo> procs = am.getRunningAppProcesses();
        if (procs != null) {
            for (ActivityManager.RunningAppProcessInfo proc : procs) {
                if (proc.processName.equals(targetProcessName)) {
                    android.os.Process.killProcess(proc.pid);
                    break;
                }
            }
        }
    }

    public static void installApk(final Activity context, NativeJSObject message, final EventCallback callback) {
        final JSONObject messageData;

        // We get the manifest url out of javascript here so we can use it as a checksum
        // in a minute, when a package has been installed.
        String manifestUrl = null;
        String filePath = null;

        try {
            filePath = message.getString("filePath");
            messageData = new JSONObject(message.getObject("data").toString());
            manifestUrl = messageData.getJSONObject("app").getString("manifestURL");
        } catch (JSONException e) {
            Log.wtf(LOGTAG, "Error getting file path and data", e);
            callback.sendError("Error getting file path and data: " + e.toString());
            return;
        }

        // We will check the manifestUrl from the one in the APK.
        // Thus, we can have a one-to-one mapping of apk to receiver.
        final InstallListener receiver = new InstallListener(manifestUrl, messageData);

        // Listen for packages being installed.
        IntentFilter filter = new IntentFilter(Intent.ACTION_PACKAGE_ADDED);
        filter.addDataScheme("package");
        context.registerReceiver(receiver, filter);

        File file = new File(filePath);
        if (!file.exists()) {
            Log.wtf(LOGTAG, "APK file doesn't exist at path " + filePath);
            callback.sendError("APK file doesn't exist at path " + filePath);
            return;
        }

        Intent intent = new Intent(Intent.ACTION_VIEW);
        intent.setDataAndType(Uri.fromFile(file), "application/vnd.android.package-archive");

        // Now call the package installer.
        ActivityHandlerHelper.startIntentForActivity(context, intent, new ActivityResultHandler() {
            @Override
            public void onActivityResult(int resultCode, Intent data) {
                // The InstallListener will catch the case where the user pressed install.
                // Now deal with if the user pressed cancel.
                if (resultCode == Activity.RESULT_CANCELED) {
                    try {
                        context.unregisterReceiver(receiver);
                        receiver.cleanup();
                    } catch (java.lang.IllegalArgumentException e) {
                        // IllegalArgumentException happens because resultCode is RESULT_CANCELED
                        // when the user presses the Done button in the install confirmation dialog,
                        // even though the install has been successful (and InstallListener already
                        // unregistered the receiver).
                        Log.e(LOGTAG, "error unregistering install receiver: ", e);
                    }
                    callback.sendError("APK installation cancelled by user");
                }
            }
        });
    }

    public static void uninstallApk(final Activity context, NativeJSObject message) {
        String packageName = message.getString("apkPackageName");
        Uri packageUri = Uri.parse("package:" + packageName);

        Intent intent;
        if (Build.VERSION.SDK_INT < 14) {
            intent = new Intent(Intent.ACTION_DELETE, packageUri);
        } else {
            intent = new Intent(Intent.ACTION_UNINSTALL_PACKAGE, packageUri);
        }

        context.startActivity(intent);
    }

    private static final int DEFAULT_VERSION_CODE = -1;

    public static JSONObject getApkVersions(Activity context, String[] packageNames) {
        Set<String> packageNameSet = new HashSet<String>();
        packageNameSet.addAll(Arrays.asList(packageNames));

        final PackageManager pm = context.getPackageManager();
        List<ApplicationInfo> apps = pm.getInstalledApplications(0);

        JSONObject jsonMessage = new JSONObject();

        for (ApplicationInfo app : apps) {
            if (packageNameSet.contains(app.packageName)) {
                int versionCode = DEFAULT_VERSION_CODE;
                try {
                    versionCode = pm.getPackageInfo(app.packageName, 0).versionCode;
                } catch (PackageManager.NameNotFoundException e) {
                    Log.e(LOGTAG, "couldn't get version for app " + app.packageName, e);
                }
                try {
                    jsonMessage.put(app.packageName, versionCode);
                } catch (JSONException e) {
                    Log.e(LOGTAG, "unable to store version code field for app " + app.packageName, e);
                }
            }
        }

        return jsonMessage;
    }
}
