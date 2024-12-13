/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.content.Context
import androidx.annotation.VisibleForTesting
import androidx.work.testing.WorkManagerTestInitHelper
import mozilla.telemetry.glean.Glean
import mozilla.telemetry.glean.config.Configuration
import org.junit.rules.TestWatcher
import org.junit.runner.Description
import org.mozilla.fenix.GleanMetrics.Pings

/**
 * This implements a JUnit rule for writing tests for Glean SDK metrics.
 *
 * The rule takes care of resetting the Glean SDK between tests and
 * initializing all the required dependencies.
 *
 * Example usage:
 *
 * ```
 * // Add the following lines to you test class.
 * @get:Rule
 * val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())
 * ```
 *
 * @param context the application context
 * @param configToUse an optional [Configuration] to initialize the Glean SDK with
 */
@VisibleForTesting(otherwise = VisibleForTesting.NONE)
class FenixGleanTestRule(
    val context: Context,
    val configToUse: Configuration = Configuration(),
) : TestWatcher() {
    /**
     * Invoked when a test is about to start.
     */
    override fun starting(description: Description?) {
        // We're using the WorkManager in a bunch of places, and Glean will crash
        // in tests without this line. Let's simply put it here.
        WorkManagerTestInitHelper.initializeTestWorkManager(context)

        /**
         * Always skip the first metrics ping, which would otherwise be overdue.
         * Tests should explicitly destroy Glean and recreate it to test the metrics ping scheduler.
         * This is the same as `delayMetricsPing` from `TestUtils.kt`,
         * but now part of the publicly available test rule.
         */

//        // Set the current system time to a known datetime.
//        val fakeNow = Calendar.getInstance()
//        fakeNow.clear()
//        @Suppress("MagicNumber") // it's a fixed date only used in tests.
//        fakeNow.set(2015, 6, 11, 2, 0, 0)
//        SystemClock.setCurrentTimeMillis(fakeNow.timeInMillis)
//
//        // Set the last sent date to yesterday.
//        val buildInfo = BuildInfo(versionCode = "0.0.1", versionName = "0.0.1", buildDate = Calendar.getInstance())
//        val mps = MetricsPingScheduler(context, buildInfo)
//
//        mps.updateSentDate(getISOTimeString(fakeNow, truncateTo = TimeUnit.DAY))

        Glean.registerPings(Pings)

        Glean.resetGlean(
            context = context,
            config = configToUse,
            clearStores = true,
        )
    }

    override fun finished(description: Description?) {
        // This closes the database to help prevent leaking it during tests.
        // See Bug1719905 for more info.
        WorkManagerTestInitHelper.closeWorkDatabase()

        super.finished(description)
    }
}
