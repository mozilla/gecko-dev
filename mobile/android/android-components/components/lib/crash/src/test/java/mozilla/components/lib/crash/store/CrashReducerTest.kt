/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.store

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class CrashReducerTest {
    @Test
    fun `middlewear actions don't manipulate the state`() {
        listOf(
            CrashAction.Initialize,
            CrashAction.CheckDeferred,
            CrashAction.CheckForCrashes,
            CrashAction.FinishCheckingForCrashes(hasUnsentCrashes = true),
            CrashAction.FinishCheckingForCrashes(hasUnsentCrashes = false),
        ).forEach {
            assertEquals(crashReducer(CrashState.Idle, it), CrashState.Idle)
        }
    }

    @Test
    fun `GIVEN an Idle state WHEN we process a RestoreDefer action with a value THEN update state to Deferred`() {
        val state = crashReducer(CrashState.Idle, CrashAction.RestoreDeferred(now = 1L, until = 45L))
        assertEquals(state, CrashState.Deferred(until = 45L))
    }

    @Test
    fun `GIVEN an Idle state WHEN we process a RestoreDefer action without now value greater than until THEN update state to Ready`() {
        val state = crashReducer(CrashState.Idle, CrashAction.RestoreDeferred(now = 45L, until = 1L))
        assertEquals(state, CrashState.Ready)
    }

    @Test
    fun `GIVEN an Idle state WHEN we process a Defer action THEN update state to Deferred + 5 days`() {
        val state = crashReducer(CrashState.Idle, CrashAction.Defer(now = 0L))
        assertEquals(state, CrashState.Deferred(until = 432000000))
    }

    @Test
    fun `GIVEN a Ready state WHEN we process a ShowPrompt action THEN update state to Reporting`() {
        val state = crashReducer(CrashState.Idle, CrashAction.ShowPrompt)
        assertEquals(state, CrashState.Reporting)
    }

    @Test
    fun `GIVEN a Reporting state WHEN we process a CancelTapped or ReportTapped action THEN update state to Done`() {
        listOf(
            CrashAction.CancelTapped,
            CrashAction.ReportTapped(automaticallySendChecked = true),
        ).forEach {
            assertEquals(crashReducer(CrashState.Reporting, it), CrashState.Done)
        }
    }
}
