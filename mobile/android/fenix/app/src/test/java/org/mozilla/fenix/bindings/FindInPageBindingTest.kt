/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.bindings

import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.FindInPageAction

class FindInPageBindingTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN find in page started action is dispatched THEN launch find in page feature`() = runTestOnMain {
        val appStore = AppStore()
        var onFindInPageLaunchCalled = false

        val binding = FindInPageBinding(
            appStore = appStore,
            onFindInPageLaunch = { onFindInPageLaunchCalled = true },
        )
        binding.start()

        appStore.dispatch(FindInPageAction.FindInPageStarted)

        // Wait for FindInPageAction.FindInPageStarted
        appStore.waitUntilIdle()
        // Wait for FindInPageAction.FindInPageShown
        appStore.waitUntilIdle()

        assertFalse(appStore.state.showFindInPage)

        assertTrue(onFindInPageLaunchCalled)
    }
}
