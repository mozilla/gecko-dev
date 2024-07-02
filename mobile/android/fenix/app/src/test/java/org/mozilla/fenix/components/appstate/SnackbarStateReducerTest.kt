/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.SnackbarAction
import org.mozilla.fenix.components.appstate.snackbar.SnackbarState

class SnackbarStateReducerTest {

    @Test
    fun `WHEN snackbar dismissed action is dispatched THEN state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(SnackbarAction.SnackbarDismissed).joinBlocking()

        assertEquals(SnackbarState.Dismiss, appStore.state.snackbarState)
    }

    @Test
    fun `WHEN snackbar shown action is dispatched THEN state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(SnackbarAction.SnackbarShown).joinBlocking()

        assertEquals(SnackbarState.None, appStore.state.snackbarState)
    }

    @Test
    fun `WHEN reset action is dispatched THEN state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(SnackbarAction.Reset).joinBlocking()

        assertEquals(SnackbarState.None, appStore.state.snackbarState)
    }
}
