/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.tabstrip

import android.content.Context
import android.content.pm.PackageManager
import android.content.res.Resources
import android.os.Build
import android.util.DisplayMetrics
import android.util.Size
import android.view.WindowManager
import androidx.annotation.RequiresApi
import androidx.core.content.ContextCompat
import org.mozilla.fenix.Config
import kotlin.math.min

private const val LARGE_SCREEN_THRESHOLD_DP = 600

/**
 * Returns true if the tab strip is enabled.
 */
fun Context.isTabStripEnabled(): Boolean =
    isTabStripEligible() && Config.channel.isNightlyOrDebug

/**
 * Returns true if the the device has the prerequisites to enable the tab strip.
 */
private fun Context.isTabStripEligible(): Boolean =
    isLargeScreenSize() && !doesDeviceHaveHinge()

/**
 * Check if the device has a hinge sensor.
 */
private fun Context.doesDeviceHaveHinge(): Boolean =
    Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
        packageManager.hasSystemFeature(PackageManager.FEATURE_SENSOR_HINGE_ANGLE)

/**
 * Returns true if the device has a large screen size. This is determined by the smallest width
 * of the screen (not window). This value will not change over the course of the app's lifecycle.
 */
private fun Context.isLargeScreenSize(): Boolean =
    smallestScreenWidthDp() >= LARGE_SCREEN_THRESHOLD_DP

/**
 * Returns the smallest width of the screen in dp.
 */
private fun Context.smallestScreenWidthDp(): Float {
    val size = ScreenMetricsCompat.getScreenSize(this)

    return min(size.width, size.height) / resources.displayMetrics.density
}

/**
 * Compat utility to get screen metrics.
 */
private object ScreenMetricsCompat {

    private val screenMetricsApi: ScreenMetricsApi =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            ScreenMetricsApiApi30Impl()
        } else {
            ScreenMetricsApiImpl()
        }

    /**
     * @see [ScreenMetricsApi.getScreenSize]
     */
    fun getScreenSize(context: Context): Size = screenMetricsApi.getScreenSize(context)

    /**
     * Interface to abstract the implementation of the screen metrics API based on the SDK version.
     */
    private interface ScreenMetricsApi {

        /**
         * Returns screen [Size] in pixels.
         *
         * @param context [Context] to get the display metrics from. Use a visual Context from
         * Activity or View.
         */
        fun getScreenSize(context: Context): Size
    }

    /**
     * Implementation of the screen metrics API for API 30 and above.
     */
    @RequiresApi(Build.VERSION_CODES.R)
    private class ScreenMetricsApiApi30Impl : ScreenMetricsApi {

        /**
         * @see [ScreenMetricsApi.getScreenSize]
         */
        override fun getScreenSize(context: Context): Size {
            val windowManager = ContextCompat.getSystemService(context, WindowManager::class.java)
            if (windowManager != null) {
                val windowMetrics = windowManager.maximumWindowMetrics
                return Size(windowMetrics.bounds.width(), windowMetrics.bounds.height())
            } else {
                // Fallback to display metrics if WindowManager is not available.
                val displayMetrics = Resources.getSystem().displayMetrics
                return Size(displayMetrics.widthPixels, displayMetrics.heightPixels)
            }
        }
    }

    /**
     * Implementation of the screen metrics API for API 29 and below.
     */
    private class ScreenMetricsApiImpl : ScreenMetricsApi {

        /**
         * @see [ScreenMetricsApi.getScreenSize]
         */
        @Suppress("DEPRECATION")
        override fun getScreenSize(context: Context): Size {
            val display =
                ContextCompat.getSystemService(context, WindowManager::class.java)?.defaultDisplay
            val metrics: DisplayMetrics = if (display != null) {
                DisplayMetrics().also {
                    display.getRealMetrics(it)
                }
            } else {
                Resources.getSystem().displayMetrics
            }
            return Size(metrics.widthPixels, metrics.heightPixels)
        }
    }
}
