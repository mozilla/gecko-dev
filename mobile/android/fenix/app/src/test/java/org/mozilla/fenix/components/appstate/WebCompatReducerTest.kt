/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.appstate.AppAction.WebCompatAction
import org.mozilla.fenix.components.appstate.snackbar.SnackbarState
import org.mozilla.fenix.components.appstate.webcompat.WebCompatReducer
import org.mozilla.fenix.components.appstate.webcompat.WebCompatState

class WebCompatReducerTest {

    @Test
    fun `WHEN the web compat state has an update THEN web compat data AppState should be updated`() {
        val webCompatState = WebCompatState(
            tabUrl = "www.mozilla.org",
            enteredUrl = "www.mozilla.org/3",
            reason = "slow",
            problemDescription = "problem description",
        )
        val expected = AppState(
            webCompatState = webCompatState,
        )
        val actual = WebCompatReducer.reduce(
            state = AppState(),
            action = WebCompatAction.WebCompatStateUpdated(newState = webCompatState),
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN the web compat state is reset THEN the web compat data in AppState should be reset`() {
        val expected = AppState()
        val actual = WebCompatReducer.reduce(
            state = AppState(
                webCompatState = WebCompatState(
                    tabUrl = "www.mozilla.org",
                    enteredUrl = "www.mozilla.org/3",
                    reason = "slow",
                    problemDescription = "problem description",
                ),
            ),
            action = WebCompatAction.WebCompatStateReset,
        )

        assertEquals(expected, actual)
    }

    @Test
    fun `WHEN the WebCompat report is successfully submitted THEN the snackbar state should be updated and the web compat data should be reset`() {
        val appState = AppState(
            webCompatState = WebCompatState(
                tabUrl = "www.mozilla.org",
                enteredUrl = "www.mozilla.org/3",
                reason = "slow",
                problemDescription = "problem description",
            ),
            snackbarState = SnackbarState.None,
        )

        val actual = WebCompatReducer.reduce(appState, WebCompatAction.WebCompatReportSent)

        assertEquals(null, actual.webCompatState)
        assertEquals(SnackbarState.WebCompatReportSent, actual.snackbarState)
    }
}
