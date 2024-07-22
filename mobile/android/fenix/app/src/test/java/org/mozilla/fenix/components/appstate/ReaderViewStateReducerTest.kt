/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.ReaderViewAction
import org.mozilla.fenix.components.appstate.readerview.ReaderViewState

class ReaderViewStateReducerTest {

    @Test
    fun `WHEN reader view started action is dispatched THEN reader view state is updated`() {
        val appStore = AppStore()
        appStore.dispatch(ReaderViewAction.ReaderViewStarted).joinBlocking()
        assertEquals(ReaderViewState.Active, appStore.state.readerViewState)
    }

    @Test
    fun `WHEN reader view dismissed action is dispatched THEN reader view state is updated`() {
        val appStore = AppStore()
        appStore.dispatch(ReaderViewAction.ReaderViewDismissed).joinBlocking()
        assertEquals(ReaderViewState.Dismiss, appStore.state.readerViewState)
    }

    @Test
    fun `WHEN reader view controls shown action is dispatched THEN reader view state is updated`() {
        val appStore = AppStore()
        appStore.dispatch(ReaderViewAction.ReaderViewControlsShown).joinBlocking()
        assertEquals(ReaderViewState.ShowControls, appStore.state.readerViewState)
    }

    @Test
    fun `WHEN reader view reset action is dispatched THEN reader view state is updated`() {
        val appStore = AppStore()
        appStore.dispatch(ReaderViewAction.Reset).joinBlocking()
        assertEquals(ReaderViewState.None, appStore.state.readerViewState)
    }
}
