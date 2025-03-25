/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.messaging

import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.utils.RunWhenReadyQueue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.MessagingAction

class MessagingFeatureTest {
    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN start is called and queue is not ready THEN do nothing`() = runTestOnMain {
        val appStore: AppStore = spyk(AppStore())
        val queue = RunWhenReadyQueue(this)
        val binding = MessagingFeature(
            appStore = appStore,
            surface = FenixMessageSurfaceId.HOMESCREEN,
            runWhenReadyQueue = queue,
        )

        binding.start()

        verify(exactly = 0) { appStore.dispatch(MessagingAction.Evaluate(FenixMessageSurfaceId.HOMESCREEN)) }
    }

    @Test
    fun `WHEN start is called and queue is ready THEN evaluate message`() = runTestOnMain {
        val appStore: AppStore = spyk(AppStore())
        val queue = RunWhenReadyQueue(this)
        val binding = MessagingFeature(
            appStore = appStore,
            surface = FenixMessageSurfaceId.HOMESCREEN,
            runWhenReadyQueue = queue,
        )

        binding.start()
        queue.ready()

        verify { appStore.dispatch(MessagingAction.Evaluate(FenixMessageSurfaceId.HOMESCREEN)) }
    }
}
