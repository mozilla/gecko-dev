/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.FindInPageAction

class FindInPageStateReducerTest {

    @Test
    fun `WHEN find in page started action is dispatched THEN state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(FindInPageAction.FindInPageStarted).joinBlocking()

        assertTrue(appStore.state.showFindInPage)
    }

    @Test
    fun `WHEN find in page dismissed action is dispatched THEN state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(FindInPageAction.FindInPageDismissed).joinBlocking()

        assertFalse(appStore.state.showFindInPage)
    }

    @Test
    fun `WHEN find in page shown action is dispatched THEN state is updated`() {
        val appStore = AppStore()

        appStore.dispatch(FindInPageAction.FindInPageShown).joinBlocking()

        assertFalse(appStore.state.showFindInPage)
    }
}
