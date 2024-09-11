/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.crash.store.CrashAction
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction

@RunWith(AndroidJUnit4::class)
class CrashReporterBindingTest {
    @get:Rule
    val coroutineRule = MainCoroutineRule()

    @Test
    fun `GIVEN WHEN THEN`() = runTestOnMain {
        val appStore = AppStore()
        var onReportingCalled = false
        val binding = CrashReporterBinding(
            store = appStore,
            onReporting = { onReportingCalled = true },
        )
        binding.start()

        appStore.dispatch(AppAction.CrashActionWrapper(CrashAction.ShowPrompt))
        appStore.waitUntilIdle()
        assertTrue(onReportingCalled)
    }
}
