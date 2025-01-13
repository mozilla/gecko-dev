/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.webcompat.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.launch
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.yield
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.webcompat.store.WebCompatReporterAction
import org.mozilla.fenix.webcompat.store.WebCompatReporterStore

@RunWith(AndroidJUnit4::class)
class WebCompatReporterNavigationMiddlewareTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN a navigation action is emitted before and after the flow is collected THEN only the navigation action emitted after the collection is collected`() = runTest {
        val store = WebCompatReporterStore(
            middleware = listOf(
                WebCompatReporterNavigationMiddleware(),
            ),
        )

        // This event will be dropped as no collector is subscribed when this action is dispatched
        store.dispatch(WebCompatReporterAction.BackPressed)

        // backgroundScope is used so the coroutine is cancelled when the test finishes.
        // If this is not used, then the test will never finish, since sharedFlow is a hot flow and never completes
        backgroundScope.launch {
            store.navEvents.collect {
                val expectedAction = WebCompatReporterAction.CancelClicked
                assertEquals(expectedAction, it)
            }
        }

        // Launch a new coroutine to dispatch events. Ensure this is not finished before the
        // collector has processed the event
        launch {
            store.dispatch(WebCompatReporterAction.CancelClicked)

            // Yield the thread so the event can be processed by the collector before the test finishes.
            // If this is not done, the test will finish before the collector has processed the event.
            yield()
        }
    }
}
