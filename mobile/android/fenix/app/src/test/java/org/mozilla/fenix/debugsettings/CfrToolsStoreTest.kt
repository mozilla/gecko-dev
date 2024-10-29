/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsAction
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsState
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsStore

@RunWith(AndroidJUnit4::class)
class CfrToolsStoreTest {

    @Test
    fun `WHEN the init action is dispatched THEN the state remains the same`() {
        val initialState = CfrToolsState()
        val store = CfrToolsStore(
            initialState = initialState,
        )
        store.dispatch(CfrToolsAction.Init)
        assertEquals(initialState, store.state)
    }

    @Test
    fun `GIVEN the homepage sync CFR has been shown WHEN the homepage sync CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSyncShown = true,
            ),
        )

        assertTrue(store.state.homepageSyncShown)
        store.dispatch(CfrToolsAction.HomepageSyncShownToggled)
        assertFalse(store.state.homepageSyncShown)
    }

    @Test
    fun `GIVEN the homepage sync CFR has not been shown WHEN the homepage sync CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSyncShown = false,
            ),
        )

        assertFalse(store.state.homepageSyncShown)
        store.dispatch(CfrToolsAction.HomepageSyncShownToggled)
        assertTrue(store.state.homepageSyncShown)
    }

    @Test
    fun `GIVEN the homepage nav toolbar CFR has been shown WHEN the homepage nav toolbar CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageNavToolbarShown = true,
            ),
        )

        assertTrue(store.state.homepageNavToolbarShown)
        store.dispatch(CfrToolsAction.HomepageNavToolbarShownToggled)
        assertFalse(store.state.homepageNavToolbarShown)
    }

    @Test
    fun `GIVEN the homepage nav toolbar CFR has not been shown WHEN the homepage nav toolbar CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageNavToolbarShown = false,
            ),
        )

        assertFalse(store.state.homepageNavToolbarShown)
        store.dispatch(CfrToolsAction.HomepageNavToolbarShownToggled)
        assertTrue(store.state.homepageNavToolbarShown)
    }

    @Test
    fun `GIVEN the nav buttons CFR has been shown WHEN the nav buttons CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                navButtonsShown = true,
            ),
        )

        assertTrue(store.state.navButtonsShown)
        store.dispatch(CfrToolsAction.NavButtonsShownToggled)
        assertFalse(store.state.navButtonsShown)
    }

    @Test
    fun `GIVEN the nav buttons CFR has not been shown WHEN the nav buttons CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                navButtonsShown = false,
            ),
        )

        assertFalse(store.state.navButtonsShown)
        store.dispatch(CfrToolsAction.NavButtonsShownToggled)
        assertTrue(store.state.navButtonsShown)
    }

    @Test
    fun `GIVEN the add private tab to home CFR has been shown WHEN the add private tab to home CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                addPrivateTabToHomeShown = true,
            ),
        )

        assertTrue(store.state.addPrivateTabToHomeShown)
        store.dispatch(CfrToolsAction.AddPrivateTabToHomeShownToggled)
        assertFalse(store.state.addPrivateTabToHomeShown)
    }

    @Test
    fun `GIVEN the add private tab to home CFR has not been shown WHEN the add private tab to home CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                addPrivateTabToHomeShown = false,
            ),
        )

        assertFalse(store.state.addPrivateTabToHomeShown)
        store.dispatch(CfrToolsAction.AddPrivateTabToHomeShownToggled)
        assertTrue(store.state.addPrivateTabToHomeShown)
    }

    @Test
    fun `GIVEN the tab auto close banner CFR has been shown WHEN the tab auto close banner CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tabAutoCloseBannerShown = true,
            ),
        )

        assertTrue(store.state.tabAutoCloseBannerShown)
        store.dispatch(CfrToolsAction.TabAutoCloseBannerShownToggled)
        assertFalse(store.state.tabAutoCloseBannerShown)
    }

    @Test
    fun `GIVEN the tab auto close banner CFR has not been shown WHEN the tab auto close banner CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tabAutoCloseBannerShown = false,
            ),
        )

        assertFalse(store.state.tabAutoCloseBannerShown)
        store.dispatch(CfrToolsAction.TabAutoCloseBannerShownToggled)
        assertTrue(store.state.tabAutoCloseBannerShown)
    }

    @Test
    fun `GIVEN the inactive tabs CFR has been shown WHEN the inactive tabs CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                inactiveTabsShown = true,
            ),
        )

        assertTrue(store.state.inactiveTabsShown)
        store.dispatch(CfrToolsAction.InactiveTabsShownToggled)
        assertFalse(store.state.inactiveTabsShown)
    }

    @Test
    fun `GIVEN the inactive tabs CFR has not been shown WHEN the inactive tabs CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                inactiveTabsShown = false,
            ),
        )

        assertFalse(store.state.inactiveTabsShown)
        store.dispatch(CfrToolsAction.InactiveTabsShownToggled)
        assertTrue(store.state.inactiveTabsShown)
    }

    @Test
    fun `GIVEN the open in app CFR has been shown WHEN the open in app CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                openInAppShown = true,
            ),
        )

        assertTrue(store.state.openInAppShown)
        store.dispatch(CfrToolsAction.OpenInAppShownToggled)
        assertFalse(store.state.openInAppShown)
    }

    @Test
    fun `GIVEN the open in app CFR has not been shown WHEN the open in app CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                openInAppShown = false,
            ),
        )

        assertFalse(store.state.openInAppShown)
        store.dispatch(CfrToolsAction.OpenInAppShownToggled)
        assertTrue(store.state.openInAppShown)
    }

    @Test
    fun `GIVEN the PWA dialog CFR has been shown WHEN the PWA dialog CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                pwaShown = true,
            ),
        )

        assertTrue(store.state.pwaShown)
        store.dispatch(CfrToolsAction.PwaShownToggled)
        assertFalse(store.state.pwaShown)
    }

    @Test
    fun `GIVEN the PWA dialog CFR has not been shown WHEN the PWA dialog CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                pwaShown = false,
            ),
        )

        assertFalse(store.state.pwaShown)
        store.dispatch(CfrToolsAction.PwaShownToggled)
        assertTrue(store.state.pwaShown)
    }

    @Test
    fun `WHEN the last CFR shown timestamp action is reset THEN the state remains the same`() {
        val store = CfrToolsStore()

        val previousState = store.state
        store.dispatch(CfrToolsAction.ResetLastCFRTimestampButtonClicked)
        assertEquals(previousState, store.state)
    }

    @Test
    fun `GIVEN the homepage sync CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSyncShown = false,
            ),
        )

        assertFalse(store.state.homepageSyncShown)
        store.dispatch(CfrToolsAction.HomepageSyncCfrUpdated(true))
        assertTrue(store.state.homepageSyncShown)
    }

    @Test
    fun `GIVEN the homepage sync CFR has been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSyncShown = true,
            ),
        )

        assertTrue(store.state.homepageSyncShown)
        store.dispatch(CfrToolsAction.HomepageSyncCfrUpdated(false))
        assertFalse(store.state.homepageSyncShown)
    }

    @Test
    fun `GIVEN the homepage nav toolbar CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageNavToolbarShown = false,
            ),
        )

        assertFalse(store.state.homepageNavToolbarShown)
        store.dispatch(CfrToolsAction.HomepageNavToolbarCfrUpdated(true))
        assertTrue(store.state.homepageNavToolbarShown)
    }

    @Test
    fun `GIVEN the homepage nav toolbar CFR has been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageNavToolbarShown = true,
            ),
        )

        assertTrue(store.state.homepageNavToolbarShown)
        store.dispatch(CfrToolsAction.HomepageNavToolbarCfrUpdated(false))
        assertFalse(store.state.homepageNavToolbarShown)
    }

    @Test
    fun `GIVEN the nav buttons CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                navButtonsShown = false,
            ),
        )

        assertFalse(store.state.navButtonsShown)
        store.dispatch(CfrToolsAction.NavButtonsCfrUpdated(true))
        assertTrue(store.state.navButtonsShown)
    }

    @Test
    fun `GIVEN the nav buttons CFR has been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                navButtonsShown = true,
            ),
        )

        assertTrue(store.state.navButtonsShown)
        store.dispatch(CfrToolsAction.NavButtonsCfrUpdated(false))
        assertFalse(store.state.navButtonsShown)
    }

    @Test
    fun `GIVEN the add private tab to home CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                addPrivateTabToHomeShown = false,
            ),
        )

        assertFalse(store.state.addPrivateTabToHomeShown)
        store.dispatch(CfrToolsAction.AddPrivateTabToHomeCfrUpdated(true))
        assertTrue(store.state.addPrivateTabToHomeShown)
    }

    @Test
    fun `GIVEN the add private tab to home CFR has been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                addPrivateTabToHomeShown = true,
            ),
        )

        assertTrue(store.state.addPrivateTabToHomeShown)
        store.dispatch(CfrToolsAction.AddPrivateTabToHomeCfrUpdated(false))
        assertFalse(store.state.addPrivateTabToHomeShown)
    }

    @Test
    fun `GIVEN the tab auto close banner CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tabAutoCloseBannerShown = false,
            ),
        )

        assertFalse(store.state.tabAutoCloseBannerShown)
        store.dispatch(CfrToolsAction.TabAutoCloseBannerCfrUpdated(true))
        assertTrue(store.state.tabAutoCloseBannerShown)
    }

    @Test
    fun `GIVEN the tab auto close banner CFR has been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tabAutoCloseBannerShown = true,
            ),
        )

        assertTrue(store.state.tabAutoCloseBannerShown)
        store.dispatch(CfrToolsAction.TabAutoCloseBannerCfrUpdated(false))
        assertFalse(store.state.tabAutoCloseBannerShown)
    }

    @Test
    fun `GIVEN the inactive tabs CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                inactiveTabsShown = false,
            ),
        )

        assertFalse(store.state.inactiveTabsShown)
        store.dispatch(CfrToolsAction.InactiveTabsCfrUpdated(true))
        assertTrue(store.state.inactiveTabsShown)
    }

    @Test
    fun `GIVEN the inactive tabs CFR has been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                inactiveTabsShown = true,
            ),
        )

        assertTrue(store.state.inactiveTabsShown)
        store.dispatch(CfrToolsAction.InactiveTabsCfrUpdated(false))
        assertFalse(store.state.inactiveTabsShown)
    }

    @Test
    fun `GIVEN the open in app CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                openInAppShown = false,
            ),
        )

        assertFalse(store.state.openInAppShown)
        store.dispatch(CfrToolsAction.OpenInAppCfrUpdated(true))
        assertTrue(store.state.openInAppShown)
    }

    @Test
    fun `GIVEN the open in app CFR has been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                openInAppShown = true,
            ),
        )

        assertTrue(store.state.openInAppShown)
        store.dispatch(CfrToolsAction.OpenInAppCfrUpdated(false))
        assertFalse(store.state.openInAppShown)
    }

    @Test
    fun `GIVEN the PWA CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                pwaShown = false,
            ),
        )

        assertFalse(store.state.pwaShown)
        store.dispatch(CfrToolsAction.PwaCfrUpdated(true))
        assertTrue(store.state.pwaShown)
    }

    @Test
    fun `GIVEN the PWA CFR has been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                pwaShown = true,
            ),
        )

        assertTrue(store.state.pwaShown)
        store.dispatch(CfrToolsAction.PwaCfrUpdated(false))
        assertFalse(store.state.pwaShown)
    }
}
