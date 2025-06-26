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
    fun `GIVEN the homepage searchbar CFR has been shown WHEN the homepage searchbar CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSearchBarShown = true,
            ),
        )

        assertTrue(store.state.homepageSearchBarShown)
        store.dispatch(CfrToolsAction.HomepageSearchBarShownToggled)
        assertFalse(store.state.homepageSearchBarShown)
    }

    @Test
    fun `GIVEN the homepage searchbar CFR has not been shown WHEN the homepage searchbar CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSearchBarShown = false,
            ),
        )

        assertFalse(store.state.homepageSearchBarShown)
        store.dispatch(CfrToolsAction.HomepageSearchBarShownToggled)
        assertTrue(store.state.homepageSearchBarShown)
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
    fun `GIVEN the homepage searchbar CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSearchBarShown = false,
            ),
        )

        assertFalse(store.state.homepageSearchBarShown)
        store.dispatch(CfrToolsAction.HomepageSearchbarCfrLoaded(true))
        assertTrue(store.state.homepageSearchBarShown)
    }

    @Test
    fun `GIVEN the homepage searchbar CFR has been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSearchBarShown = true,
            ),
        )

        assertTrue(store.state.homepageSearchBarShown)
        store.dispatch(CfrToolsAction.HomepageSearchbarCfrLoaded(false))
        assertFalse(store.state.homepageSearchBarShown)
    }

    @Test
    fun `GIVEN the tab auto close banner CFR has not been shown WHEN the corresponding CfrPreferenceUpdate is dispatched THEN update its state to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tabAutoCloseBannerShown = false,
            ),
        )

        assertFalse(store.state.tabAutoCloseBannerShown)
        store.dispatch(CfrToolsAction.TabAutoCloseBannerCfrLoaded(true))
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
        store.dispatch(CfrToolsAction.TabAutoCloseBannerCfrLoaded(false))
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
        store.dispatch(CfrToolsAction.InactiveTabsCfrLoaded(true))
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
        store.dispatch(CfrToolsAction.InactiveTabsCfrLoaded(false))
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
        store.dispatch(CfrToolsAction.OpenInAppCfrLoaded(true))
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
        store.dispatch(CfrToolsAction.OpenInAppCfrLoaded(false))
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
        store.dispatch(CfrToolsAction.PwaCfrLoaded(true))
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
        store.dispatch(CfrToolsAction.PwaCfrLoaded(false))
        assertFalse(store.state.pwaShown)
    }
}
