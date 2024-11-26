/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("DEPRECATION")

package org.mozilla.fenix.perf

import android.app.ActivityManager
import android.content.Context
import android.content.SharedPreferences
import android.os.Process
import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ActivityTestRule
import mozilla.telemetry.glean.testing.GleanTestLocalServer
import org.hamcrest.CoreMatchers.anyOf
import org.hamcrest.CoreMatchers.`is`
import org.hamcrest.MatcherAssert.assertThat
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Ignore
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.GleanMetrics.AppExitInfo
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.helpers.HomeActivityTestRule
import org.mozilla.fenix.helpers.MockWebServerHelper
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class ApplicationExitInfoMetricsTest {

    private val server = MockWebServerHelper.createAlwaysOkMockWebServer()

    @get:Rule
    val activityRule: ActivityTestRule<HomeActivity> = HomeActivityTestRule(skipOnboarding = true)

    @get:Rule
    val gleanRule = GleanTestLocalServer(ApplicationProvider.getApplicationContext(), server.port)

    private lateinit var appContext: Context
    private lateinit var activityManager: ActivityManager

    @Before
    fun setup() {
        appContext = InstrumentationRegistry.getInstrumentation().targetContext
        activityManager = appContext.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager

        preferences(appContext)
            .edit()
            .clear()
            .apply()
    }

    @Test(timeout = 30000) // adding timeout to make sure process kill does not cause infinite loop
    fun recordProcessExitsShouldUpdateSharedPreferenceWhenKillSignalSentToChildProcess() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync()

        assertEquals(-1, getLastHandledTime(appContext))

        val processId = maybeKillChildProcess("tab")

        val processExits = activityManager.getHistoricalProcessExitReasons(null, 0, 0)
        assertTrue(processExits.isNotEmpty())
        assertEquals(processId, processExits[0].pid)

        ApplicationExitInfoMetrics.recordProcessExits(appContext)

        assertEquals(getLastHandledTime(appContext), processExits[0].timestamp)
    }

    @Test(timeout = 30000) // adding timeout to make sure process kill does not cause infinite loop
    fun recordProcessExitsShouldRecordProcessKillWhenKillSignalSentToChildProcesses() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync()

        assertEquals(-1, getLastHandledTime(appContext)) // sanity check for startup

        maybeKillChildProcess("tab")
        maybeKillChildProcess("gpu")

        val historicalProcessExits = activityManager.getHistoricalProcessExitReasons(null, 0, 0)
        historicalProcessExits.retainAll {
            ApplicationExitInfoMetrics.TRACKED_REASONS.contains(it?.reason)
        }

        ApplicationExitInfoMetrics.recordProcessExits(appContext)

        assertNotNull(AppExitInfo.processExited.testGetValue())
        val recordedEvents = AppExitInfo.processExited.testGetValue()!!
        assertThat(recordedEvents[0].extra!!["process_type"], anyOf(`is`("content"), `is`("gpu")))
        assertThat(recordedEvents[1].extra!!["process_type"], anyOf(`is`("content"), `is`("gpu")))
        assertEquals(getLastHandledTime(appContext).toSimpleDateFormat(), recordedEvents[0].extra!!["date"])
    }

    @Ignore("Currently GleanTestLocalServer does not reset recorded events - see Bug 1787234")
    @Test(timeout = 30000) // adding timeout to make sure process kill does not cause infinite loop
    fun recordedProcessExitsShouldMatchTheNumberOfHistoricalExitReasonsWithTrackedReasons() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync()

        assertEquals(-1, getLastHandledTime(appContext)) // sanity startup check

        maybeKillChildProcess("tab")

        val historicalProcessExits = activityManager.getHistoricalProcessExitReasons(null, 0, 0)
        historicalProcessExits.retainAll {
            ApplicationExitInfoMetrics.TRACKED_REASONS.contains(it?.reason)
        }

        ApplicationExitInfoMetrics.recordProcessExits(appContext)

        assertNotNull(AppExitInfo.processExited.testGetValue())
        val recordedEvents = AppExitInfo.processExited.testGetValue()!!
        assertEquals(historicalProcessExits.size, recordedEvents.size)
    }

    private fun maybeKillChildProcess(processType: String): Int? {
        val processId = when (processType) {
            "gpu",
            "tab",
            -> activityManager.runningAppProcesses.firstOrNull { it.processName.contains(":$processType") }?.pid
            else -> null
        }
        processId?.let {
            Process.killProcess(it)
            // make sure kill signal is sent and process is killed
            var isProcessStillAlive = true
            while (isProcessStillAlive) {
                isProcessStillAlive = isProcessStillAlive(it)
                Thread.sleep(100)
            }
        }
        return processId
    }

    private fun isProcessStillAlive(recentlyKilledProcessId: Int): Boolean {
        return activityManager.runningAppProcesses.any { it.pid == recentlyKilledProcessId }
    }

    private fun getLastHandledTime(context: Context): Long {
        return preferences(context).getLong(
            context.getPreferenceKey(R.string.pref_key_application_exit_info_last_handled_time),
            -1,
        )
    }

    private fun Long.toSimpleDateFormat(): String {
        val date = Date(this)
        return SimpleDateFormat("yyyy-MM-dd", Locale.US).format(date)
    }

    private fun preferences(context: Context): SharedPreferences =
        context.getSharedPreferences(ApplicationExitInfoMetrics.PREFERENCE_NAME, Context.MODE_PRIVATE)
}
