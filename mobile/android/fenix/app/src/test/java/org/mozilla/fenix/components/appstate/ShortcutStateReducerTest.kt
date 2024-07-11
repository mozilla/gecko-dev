/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.snackbar.SnackbarState

class ShortcutStateReducerTest {

    @Test
    fun `WHEN shortcut added action is dispatched THEN state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(AppAction.ShortcutAction.ShortcutAdded).joinBlocking()

        assertEquals(SnackbarState.ShortcutAdded, appStore.state.snackbarState)
    }

    @Test
    fun `WHEN shortcut removed action is dispatched THEN state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(AppAction.ShortcutAction.ShortcutRemoved).joinBlocking()

        assertEquals(SnackbarState.ShortcutRemoved, appStore.state.snackbarState)
    }
}
