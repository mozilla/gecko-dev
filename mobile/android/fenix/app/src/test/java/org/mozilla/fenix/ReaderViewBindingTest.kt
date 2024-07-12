/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.fenix.browser.readermode.ReaderModeController
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState

@RunWith(AndroidJUnit4::class)
class ReaderViewBindingTest {
    @get:Rule
    val coroutineRule = MainCoroutineRule()

    private lateinit var readerModeController: ReaderModeController

    @Before
    fun setUp() {
        readerModeController = mock()
    }

    @Test
    fun `WHEN the reader view active state is updated to true THEN show reader view`() = runTestOnMain {
        val appStore = AppStore()
        val binding = ReaderViewBinding(
            appStore = appStore,
            readerMenuController = readerModeController,
        )

        binding.start()

        appStore.dispatch(AppAction.UpdateReaderViewState(isReaderViewActive = true))

        // Wait for AppAction.UpdateReaderViewState
        appStore.waitUntilIdle()

        verify(readerModeController).showReaderView()
    }

    @Test
    fun `WHEN the reader view active state is updated to false THEN hide reader view`() = runTestOnMain {
        val appStore = AppStore(
            initialState = AppState(
                isReaderViewActive = true,
            ),
        )
        val binding = ReaderViewBinding(
            appStore = appStore,
            readerMenuController = readerModeController,
        )

        binding.start()

        appStore.dispatch(AppAction.UpdateReaderViewState(isReaderViewActive = false))

        // Wait for AppAction.UpdateReaderViewState
        appStore.waitUntilIdle()

        verify(readerModeController).hideReaderView()
    }
}
