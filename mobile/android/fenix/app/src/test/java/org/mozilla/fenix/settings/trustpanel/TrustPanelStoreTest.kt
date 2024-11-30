/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel

import kotlinx.coroutines.test.runTest
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
}
