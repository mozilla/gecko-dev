/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.remotesettings

import android.content.Context
import android.os.Build
import mozilla.appservices.remotesettings.RemoteSettingsConfig2
import mozilla.appservices.remotesettings.RemoteSettingsContext
import mozilla.appservices.remotesettings.RemoteSettingsServer
import mozilla.appservices.remotesettings.RemoteSettingsService
import mozilla.components.support.ktx.android.content.appName
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.json.JSONObject
import java.util.Locale
import mozilla.components.Build as AcBuild

/**
 * Wrapper around the app-services RemoteSettingsServer
 *
 * @param context [Context] that we get the storage directory from.  This is where cached records
 * get stored.
 * @param remoteSettingsServer [RemoteSettingsServer] to download from.
 */
class RemoteSettingsService(
    context: Context,
    server: RemoteSettingsServer,
    channel: String = "release",
    isLargeScreenSize: Boolean = false,
) {
    val remoteSettingsService: RemoteSettingsService by lazy {
        val appContext = generateAppContext(context, channel, isLargeScreenSize)
        val databasePath = context.getDir("remote-settings", Context.MODE_PRIVATE).absolutePath
        RemoteSettingsService(databasePath, RemoteSettingsConfig2(server = server, appContext = appContext))
    }
}

/**
 * Generate an app context to configure the `RemoteSettingsService` with.
 *
 * This is what's used for JEXL filtering.
 */
private fun generateAppContext(context: Context, channel: String, isLargeScreenSize: Boolean): RemoteSettingsContext {
    val locale = Locale.getDefault()
    val formFactor = if (isLargeScreenSize) "tablet" else "phone"
    return RemoteSettingsContext(
        appName = context.appName,
        appId = context.packageName,
        appVersion = AcBuild.version,
        channel = channel,
        deviceManufacturer = Build.MANUFACTURER,
        deviceModel = Build.MODEL,
        locale = locale.toString(),
        os = "Android",
        osVersion = Build.VERSION.RELEASE,
        androidSdkVersion = Build.VERSION.SDK_INT.toString(),
        customTargetingAttributes = JSONObject().apply {
            put("formFactor", formFactor)
            put("country", locale.country)
        },
        appBuild = null,
        architecture = Build.SUPPORTED_ABIS.get(0),
        installationDate = context.packageManager
            .getPackageInfoCompat(context.packageName, 0)
            .firstInstallTime,
        debugTag = null,
        homeDirectory = null,
    )
}
