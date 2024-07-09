/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.ActivityManager
import android.app.ActivityManager.RunningAppProcessInfo
import android.app.ApplicationExitInfo
import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import androidx.annotation.RequiresApi
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.Companion.PRIVATE
import org.mozilla.fenix.GleanMetrics.AppExitInfo
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Contains logic for recording the processes that exited in the previous sessions, i.e historical
 * [ApplicationExitInfo].
 */
object ApplicationExitInfoMetrics {

    private const val KILOBYTES_TO_MEGABYTES_CONVERSION = 1024.0

    @RequiresApi(Build.VERSION_CODES.R)
    @VisibleForTesting(otherwise = PRIVATE)
    internal val TRACKED_REASONS = listOf(
        ApplicationExitInfo.REASON_ANR,
        ApplicationExitInfo.REASON_CRASH,
        ApplicationExitInfo.REASON_CRASH_NATIVE,
        ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE,
        ApplicationExitInfo.REASON_LOW_MEMORY,
        ApplicationExitInfo.REASON_SIGNALED,
        ApplicationExitInfo.REASON_OTHER,
    )

    @VisibleForTesting(otherwise = PRIVATE)
    internal const val PREFERENCE_NAME = "app_exit_info"

    /**
     * Records process exits.
     *
     * @param context Application [Context]
     */
    @RequiresApi(Build.VERSION_CODES.R)
    fun recordProcessExits(context: Context) {
        val historicalExitReasons = getHistoricalProcessExits(context)

        // return if there's no recent process exit
        if (historicalExitReasons.isEmpty()) return

        // get last time a process exit was recorded
        val lastTimeHandled = getLastTimeHandled(context)

        // return if there's no more recent process exit than the lastTimeHandled
        if (!shouldRecordProcessExit(lastTimeHandled, historicalExitReasons[0].timestamp)) return

        // record the process exits
        record(context, lastTimeHandled, historicalExitReasons)
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun getHistoricalProcessExits(context: Context): List<ApplicationExitInfo> {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val applicationExitInfoList =
            activityManager.getHistoricalProcessExitReasons(null, 0, 0)
        applicationExitInfoList.retainAll {
            shouldRetainApplicationExitInfo(it)
        }
        return applicationExitInfoList
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun record(
        context: Context,
        lastTimeHandled: Long,
        historicalExitReasons: List<ApplicationExitInfo>,
    ) {
        for (historicalExit in historicalExitReasons) {
            // only record process exits happened after lastTimeHandled
            if (lastTimeHandled < historicalExit.timestamp) {
                AppExitInfo.processExited.record(
                    AppExitInfo.ProcessExitedExtra(
                        date = historicalExit.timestamp.toSimpleDateFormat(),
                        importance = historicalExit.importance.toProcessImportance(),
                        processType = historicalExit.processName.toProcessType(),
                        pss = historicalExit.pss.toValueInMB(),
                        rss = historicalExit.rss.toValueInMB(),
                        reason = historicalExit.reason.toProcessExitReason(),
                    ),
                )
            }
        }

        // The historical process exit info are stored in a ring buffer with a limited size.
        // This buffer persists across app sessions. We need to keep an index of the most recently
        // recorded process exit info so that we do not record the same entries in the buffer
        // multiple times.
        updateLastTimeHandled(context, historicalExitReasons[0].timestamp)
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private fun shouldRecordProcessExit(
        lastTimeHandled: Long,
        mostRecentProcessExitTimestamp: Long,
    ): Boolean {
        return mostRecentProcessExitTimestamp > lastTimeHandled
    }

    private fun shouldRetainApplicationExitInfo(
        appExitInfo: ApplicationExitInfo?,
    ): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            TRACKED_REASONS.contains(appExitInfo?.reason)
        } else {
            false
        }
    }

    private fun getLastTimeHandled(context: Context): Long {
        return preferences(context)
            .getLong(
                context.getPreferenceKey(R.string.pref_key_application_exit_info_last_handled_time),
                -1,
            )
    }

    private fun updateLastTimeHandled(
        context: Context,
        mostRecentProcessExitTimestamp: Long,
    ) {
        preferences(context)
            .edit()
            .putLong(
                context.getPreferenceKey(R.string.pref_key_application_exit_info_last_handled_time),
                mostRecentProcessExitTimestamp,
            )
            .apply()
    }

    private fun String.toProcessType(): String {
        return when {
            ":" !in this -> "parent"
            "tab" in this -> "content"
            "gpu" in this -> "gpu"
            "media" in this -> "media"
            "utility" in this -> "utility"
            else -> "other"
        }
    }

    private fun Long.toValueInMB(): Int {
        return (this / KILOBYTES_TO_MEGABYTES_CONVERSION).toInt()
    }

    private fun Int.toProcessExitReason(): String? {
        return when (this) {
            ApplicationExitInfo.REASON_ANR -> "anr"
            ApplicationExitInfo.REASON_CRASH -> "crash"
            ApplicationExitInfo.REASON_CRASH_NATIVE -> "crash_native"
            ApplicationExitInfo.REASON_LOW_MEMORY -> "low_memory"
            ApplicationExitInfo.REASON_EXCESSIVE_RESOURCE_USAGE -> "excessive_resource"
            ApplicationExitInfo.REASON_SIGNALED -> "signaled"
            ApplicationExitInfo.REASON_OTHER -> "other"
            else -> null
        }
    }

    private fun Int.toProcessImportance(): String? {
        return when (this) {
            RunningAppProcessInfo.IMPORTANCE_CACHED -> "cached"
            RunningAppProcessInfo.IMPORTANCE_CANT_SAVE_STATE -> "cant_save_state"
            RunningAppProcessInfo.IMPORTANCE_FOREGROUND -> "foreground"
            RunningAppProcessInfo.IMPORTANCE_FOREGROUND_SERVICE -> "foreground_service"
            RunningAppProcessInfo.IMPORTANCE_GONE -> "gone"
            RunningAppProcessInfo.IMPORTANCE_PERCEPTIBLE -> "perceptible"
            RunningAppProcessInfo.IMPORTANCE_SERVICE -> "service"
            RunningAppProcessInfo.IMPORTANCE_TOP_SLEEPING -> "top_sleeping"
            RunningAppProcessInfo.IMPORTANCE_VISIBLE -> "visible"
            else -> null
        }
    }

    private fun Long.toSimpleDateFormat(): String {
        val date = Date(this)
        return SimpleDateFormat("yyyy-MM-dd", Locale.US).format(date)
    }

    private fun preferences(context: Context): SharedPreferences =
        context.getSharedPreferences(PREFERENCE_NAME, Context.MODE_PRIVATE)
}
