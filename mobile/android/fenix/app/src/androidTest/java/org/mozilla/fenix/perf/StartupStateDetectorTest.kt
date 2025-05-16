/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.Activity
import androidx.lifecycle.Lifecycle
import androidx.test.core.app.ActivityScenario
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.ext.components

/**
 * Class to test the [StartupStateDetector] logic.
 */
@RunWith(AndroidJUnit4::class)
class StartupStateDetectorTest {

    @get:Rule
    val homeActivityRule = ActivityScenarioRule(HomeActivity::class.java)

    @Test
    fun testColdLaunch(): Unit = homeActivityRule.scenario.use { scenario ->

        scenario.onActivity {
            val detector = it.components.performance.startupStateDetector

            assertEquals(StartupState.COLD, detector.getStartupState())
        }
    }

    @Test
    fun testWarmLaunch() {
        val scenario = homeActivityRule.scenario

        // recreate the activity to simulate warm start
        scenario.recreate()

        scenario.onActivity {
            val detector = it.components.performance.startupStateDetector

            assertEquals(StartupState.WARM, detector.getStartupState())
        }
    }

    @Test
    fun testHotLaunch() {
        val scenario = homeActivityRule.scenario

        scenario.simulateHotStart()

        scenario.onActivity {
            val detector = it.components.performance.startupStateDetector

            assertEquals(StartupState.HOT, detector.getStartupState())
        }
    }

    @Test
    fun previousWarmAndSubsequentHotLaunchReturnsHot() {
        val scenario = homeActivityRule.scenario

        // simulate recreation
        scenario.recreate()

        // simulate backgrounding and returning
        scenario.simulateHotStart()

        scenario.onActivity {
            val detector = it.components.performance.startupStateDetector

            assertEquals(StartupState.HOT, detector.getStartupState())
        }
    }

    @Test
    fun previousHotAndSubsequentWarmLaunchReturnsWarm() {
        val scenario = homeActivityRule.scenario

        // trigger a previous hot start
        scenario.simulateHotStart()

        // trigger a warm start
        scenario.recreate()

        scenario.onActivity {
            val detector = it.components.performance.startupStateDetector
            assertEquals(StartupState.WARM, detector.getStartupState())
        }
    }

    /**
     * Simulate backgrounding the app by ensuring we are in the resumed state then transitioning
     * to created state and then to resumed state.
     *
     * @see <a href="https://developer.android.com/topic/libraries/architecture/lifecycle#lc">Android Developer Docs</a>
     *
     */
    private fun <A : Activity> ActivityScenario<A>.simulateHotStart() {
        moveToState(Lifecycle.State.RESUMED)
        moveToState(Lifecycle.State.CREATED)
        moveToState(Lifecycle.State.RESUMED)
    }
}
