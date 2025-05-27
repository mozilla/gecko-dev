/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.utils

import android.app.Activity
import android.appwidget.AppWidgetManager
import android.content.ComponentName
import android.os.Build
import org.mozilla.fenix.onboarding.WidgetPinnedReceiver
import org.mozilla.gecko.search.SearchWidgetProvider

/**
 * Displays the "add search widget" prompt for capable devices.
 *
 * @param activity the parent [Activity].
 */
fun maybeShowAddSearchWidgetPrompt(activity: Activity) {
    val appWidgetManager = AppWidgetManager.getInstance(activity)

    // Requesting to pin app widget is only available for Android 8.0 and above.
    // We don't use canShowAddSearchWidgetPrompt here directly as lint does not pick on the version check.
    if (androidVersionSupportsWidgetPinning() && appWidgetManager.isRequestPinAppWidgetSupported) {
        val searchWidgetProvider = ComponentName(activity, SearchWidgetProvider::class.java)
        val successCallback = WidgetPinnedReceiver.getPendingIntent(activity)
        appWidgetManager.requestPinAppWidget(searchWidgetProvider, null, successCallback)
    }
}

/**
 * Checks whether the device is capable of displaying the "add search widget" prompt.
 */
fun canShowAddSearchWidgetPrompt(appWidgetManager: AppWidgetManager) =
    if (androidVersionSupportsWidgetPinning()) {
        appWidgetManager.isRequestPinAppWidgetSupported
    } else {
        false
    }

/**
 * Checks whether the device Android version is capable of displaying the "add search widget" prompt.
 */
private fun androidVersionSupportsWidgetPinning() = Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
