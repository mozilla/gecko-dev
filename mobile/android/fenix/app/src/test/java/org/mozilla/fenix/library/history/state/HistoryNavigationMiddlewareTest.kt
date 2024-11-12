/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history.state

import kotlinx.coroutines.test.advanceUntilIdle
import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.library.history.HistoryFragmentAction
import org.mozilla.fenix.library.history.HistoryFragmentState
import org.mozilla.fenix.library.history.HistoryFragmentStore

class HistoryNavigationMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `GIVEN mode is editing WHEN back pressed THEN no navigation happens`() = runTest {
        var onBackPressed = false
        val middleware = HistoryNavigationMiddleware(
            onBackPressed = { onBackPressed = true },
            scope = this,
        )
        val store =
            HistoryFragmentStore(
                HistoryFragmentState.initial.copy(
                    mode = HistoryFragmentState.Mode.Editing(
                        setOf(),
                    ),
                ),
                middleware = listOf(middleware),
            )

        store.dispatch(HistoryFragmentAction.BackPressed).joinBlocking()
        advanceUntilIdle()

        assertFalse(onBackPressed)
    }

    @Test
    fun `GIVEN mode is not editing WHEN back pressed THEN onBackPressed callback invoked`() = runTest {
        var onBackPressed = false
        val middleware = HistoryNavigationMiddleware(
            onBackPressed = { onBackPressed = true },
            scope = this,
        )
        val store =
            HistoryFragmentStore(HistoryFragmentState.initial, middleware = listOf(middleware))

        store.dispatch(HistoryFragmentAction.BackPressed).joinBlocking()
        advanceUntilIdle()

        assertTrue(onBackPressed)
    }
}
