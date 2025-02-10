/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel

import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelAction
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelState
import org.mozilla.fenix.settings.trustpanel.store.TrustPanelStore

class TrustPanelStoreTest {

    @Test
    fun `WHEN toggle tracking protection action is dispatched THEN tracking protection enabled state is updated`() = runTest {
        val store = TrustPanelStore(initialState = TrustPanelState())

        store.dispatch(TrustPanelAction.ToggleTrackingProtection).join()

        assertFalse(store.state.isTrackingProtectionEnabled)
    }

    @Test
    fun `WHEN update number of trackers blocked action is dispatched THEN number of trackers blocked state is updated`() = runTest {
        val store = TrustPanelStore(initialState = TrustPanelState())

        store.dispatch(TrustPanelAction.UpdateNumberOfTrackersBlocked(1)).join()

        assertEquals(store.state.numberOfTrackersBlocked, 1)
    }

    @Test
    fun `WHEN update base domain action is dispatched THEN base domain state is updated`() = runTest {
        val store = TrustPanelStore(initialState = TrustPanelState())
        val baseDomain = "mozilla.org"

        store.dispatch(TrustPanelAction.UpdateBaseDomain(baseDomain)).join()

        assertEquals(store.state.baseDomain, baseDomain)
    }
}
