/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.activity

import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import androidx.core.net.toUri
import mozilla.components.support.utils.Browsers
import mozilla.components.support.utils.ext.resolveActivityCompat
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.OpenWith

/**
 * Helper for opening the Google Play store to install Firefox.
 */
object FirefoxInstallationHelper {
    val storeIntent = Intent(
        Intent.ACTION_VIEW,
        ("market://details?id=" + Browsers.KnownBrowser.FIREFOX.packageName).toUri(),
    )

    /**
     * Resolves the activity information for the app store installed on the device.
     *
     * @param context The application context.
     * @return The ActivityInfo of the app store activity if found and exported, null otherwise.
     */
    fun resolveAppStore(context: Context): ActivityInfo? {
        val resolveInfo =
            context.packageManager.resolveActivityCompat(storeIntent, 0) ?: return null

        return if (!resolveInfo.activityInfo.exported) {
            // We are not allowed to launch this activity.
            null
        } else {
            resolveInfo.activityInfo
        }
    }

    /**
     * Opens the application's page in the Google Play Store.
     *
     * @param context [Context] used to start the activity.
     */
    fun open(context: Context) {
        // Redirect to Google Play directly
        context.startActivity(
            storeIntent.apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
            },
        )
        OpenWith.installFirefox.record(NoExtras())
    }
}
