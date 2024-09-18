/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import androidx.test.ext.junit.runners.AndroidJUnit4
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
    fun `GIVEN the homepage sync CFR has been shown WHEN the homepage sync CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSyncShown = true,
            ),
        )

        assertTrue(store.state.homepageSyncShown)
        store.dispatch(CfrToolsAction.ToggleHomepageSyncShown)
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
        store.dispatch(CfrToolsAction.ToggleHomepageSyncShown)
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
        store.dispatch(CfrToolsAction.ToggleHomepageNavToolbarShown)
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
        store.dispatch(CfrToolsAction.ToggleHomepageNavToolbarShown)
        assertTrue(store.state.homepageNavToolbarShown)
    }

    @Test
    fun `GIVEN the wallpaper selector CFR has been shown WHEN the wallpaper selector CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                wallpaperSelectorShown = true,
            ),
        )

        assertTrue(store.state.wallpaperSelectorShown)
        store.dispatch(CfrToolsAction.ToggleWallpaperSelectorShown)
        assertFalse(store.state.wallpaperSelectorShown)
    }

    @Test
    fun `GIVEN the wallpaper selector CFR has not been shown WHEN the wallpaper selector CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                wallpaperSelectorShown = false,
            ),
        )

        assertFalse(store.state.wallpaperSelectorShown)
        store.dispatch(CfrToolsAction.ToggleWallpaperSelectorShown)
        assertTrue(store.state.wallpaperSelectorShown)
    }

    @Test
    fun `GIVEN the nav buttons CFR has been shown WHEN the nav buttons CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                navButtonsShown = true,
            ),
        )

        assertTrue(store.state.navButtonsShown)
        store.dispatch(CfrToolsAction.ToggleNavButtonsShown)
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
        store.dispatch(CfrToolsAction.ToggleNavButtonsShown)
        assertTrue(store.state.navButtonsShown)
    }

    @Test
    fun `GIVEN the TCP CFR has been shown WHEN the TCP CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tcpShown = true,
            ),
        )

        assertTrue(store.state.tcpShown)
        store.dispatch(CfrToolsAction.ToggleTcpShown)
        assertFalse(store.state.tcpShown)
    }

    @Test
    fun `GIVEN the TCP CFR has not been shown WHEN the TCP CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tcpShown = false,
            ),
        )

        assertFalse(store.state.tcpShown)
        store.dispatch(CfrToolsAction.ToggleTcpShown)
        assertTrue(store.state.tcpShown)
    }

    @Test
    fun `GIVEN the cookie banner blocker CFR has been shown WHEN the cookie banner blocker CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                cookieBannerBlockerShown = true,
            ),
        )

        assertTrue(store.state.cookieBannerBlockerShown)
        store.dispatch(CfrToolsAction.ToggleCookieBannerBlockerShown)
        assertFalse(store.state.cookieBannerBlockerShown)
    }

    @Test
    fun `GIVEN the cookie banner blocker CFR has not been shown WHEN the cookie banner blocker CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                cookieBannerBlockerShown = false,
            ),
        )

        assertFalse(store.state.cookieBannerBlockerShown)
        store.dispatch(CfrToolsAction.ToggleCookieBannerBlockerShown)
        assertTrue(store.state.cookieBannerBlockerShown)
    }

    @Test
    fun `GIVEN the cookie banners private mode CFR has been shown WHEN the cookie banners private mode CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                cookieBannersPrivateModeShown = true,
            ),
        )

        assertTrue(store.state.cookieBannersPrivateModeShown)
        store.dispatch(CfrToolsAction.ToggleCookieBannersPrivateModeShown)
        assertFalse(store.state.cookieBannersPrivateModeShown)
    }

    @Test
    fun `GIVEN the cookie banners private mode CFR has not been shown WHEN the cookie banners private mode CFR is toggled THEN its preference is set to true`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                cookieBannersPrivateModeShown = false,
            ),
        )

        assertFalse(store.state.cookieBannersPrivateModeShown)
        store.dispatch(CfrToolsAction.ToggleCookieBannersPrivateModeShown)
        assertTrue(store.state.cookieBannersPrivateModeShown)
    }

    @Test
    fun `GIVEN the add private tab to home CFR has been shown WHEN the add private tab to home CFR is toggled THEN its preference is set to false`() {
        val store = CfrToolsStore(
            initialState = CfrToolsState(
                addPrivateTabToHomeShown = true,
            ),
        )

        assertTrue(store.state.addPrivateTabToHomeShown)
        store.dispatch(CfrToolsAction.ToggleAddPrivateTabToHomeShown)
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
        store.dispatch(CfrToolsAction.ToggleAddPrivateTabToHomeShown)
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
        store.dispatch(CfrToolsAction.ToggleTabAutoCloseBannerShown)
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
        store.dispatch(CfrToolsAction.ToggleTabAutoCloseBannerShown)
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
        store.dispatch(CfrToolsAction.ToggleInactiveTabsShown)
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
        store.dispatch(CfrToolsAction.ToggleInactiveTabsShown)
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
        store.dispatch(CfrToolsAction.ToggleOpenInAppShown)
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
        store.dispatch(CfrToolsAction.ToggleOpenInAppShown)
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
        store.dispatch(CfrToolsAction.TogglePwaShown)
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
        store.dispatch(CfrToolsAction.TogglePwaShown)
        assertTrue(store.state.pwaShown)
    }
}
