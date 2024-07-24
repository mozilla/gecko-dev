/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings

import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsAction
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsPreferencesMiddleware
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsState
import org.mozilla.fenix.debugsettings.cfrs.CfrToolsStore
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class CfrToolsPreferencesMiddlewareTest {

    private lateinit var settings: Settings
    private lateinit var middleware: CfrToolsPreferencesMiddleware

    @Before
    fun setup() {
        settings = Settings(testContext)
        middleware = CfrToolsPreferencesMiddleware(
            settings = settings,
        )
    }

    @Test
    fun `GIVEN the homepage intro CFR should not be shown WHEN the toggle homepage intro CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.shouldShowJumpBackInCFR = false
        assertFalse(settings.shouldShowJumpBackInCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageIntroShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleHomepageIntroShown).joinBlocking()
        assertTrue(settings.shouldShowJumpBackInCFR)
    }

    @Test
    fun `GIVEN the homepage intro CFR should be shown WHEN the toggle homepage intro CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.shouldShowJumpBackInCFR = true
        assertTrue(settings.shouldShowJumpBackInCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageIntroShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleHomepageIntroShown).joinBlocking()
        assertFalse(settings.shouldShowJumpBackInCFR)
    }

    @Test
    fun `GIVEN the homepage sync CFR should not be shown WHEN the toggle homepage sync CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.showSyncCFR = false
        assertFalse(settings.showSyncCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSyncShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleHomepageSyncShown).joinBlocking()
        assertTrue(settings.showSyncCFR)
    }

    @Test
    fun `GIVEN the homepage sync CFR should be shown WHEN the toggle homepage sync CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.showSyncCFR = true
        assertTrue(settings.showSyncCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageSyncShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleHomepageSyncShown).joinBlocking()
        assertFalse(settings.showSyncCFR)
    }

    @Test
    fun `GIVEN the homepage nav toolbar CFR should not be shown WHEN the toggle homepage nav toolbar CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.shouldShowNavigationBarCFR = false
        assertFalse(settings.shouldShowNavigationBarCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageNavToolbarShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleHomepageNavToolbarShown).joinBlocking()
        assertTrue(settings.shouldShowNavigationBarCFR)
    }

    @Test
    fun `GIVEN the homepage nav toolbar CFR should be shown WHEN the toggle homepage nav toolbar CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.shouldShowNavigationBarCFR = true
        assertTrue(settings.shouldShowNavigationBarCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                homepageNavToolbarShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleHomepageNavToolbarShown).joinBlocking()
        assertFalse(settings.shouldShowNavigationBarCFR)
    }

    @Test
    fun `GIVEN the wallpaper selector CFR should not be shown WHEN the toggle wallpaper selector CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.showWallpaperOnboarding = false
        assertFalse(settings.showWallpaperOnboarding)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                wallpaperSelectorShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleWallpaperSelectorShown).joinBlocking()
        assertTrue(settings.showWallpaperOnboarding)
    }

    @Test
    fun `GIVEN the wallpaper selector CFR should be shown WHEN the toggle wallpaper selector CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.showWallpaperOnboarding = true
        assertTrue(settings.showWallpaperOnboarding)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                wallpaperSelectorShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleWallpaperSelectorShown).joinBlocking()
        assertFalse(settings.showWallpaperOnboarding)
    }

    @Test
    fun `GIVEN the nav buttons CFR should not be shown WHEN the toggle nav buttons CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.shouldShowNavigationButtonsCFR = false
        assertFalse(settings.shouldShowNavigationButtonsCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                navButtonsShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleNavButtonsShown).joinBlocking()
        assertTrue(settings.shouldShowNavigationButtonsCFR)
    }

    @Test
    fun `GIVEN the nav buttons CFR should be shown WHEN the toggle nav buttons CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.shouldShowNavigationButtonsCFR = true
        assertTrue(settings.shouldShowNavigationButtonsCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                navButtonsShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleNavButtonsShown).joinBlocking()
        assertFalse(settings.shouldShowNavigationButtonsCFR)
    }

    @Test
    fun `GIVEN the TCP CFR should not be shown WHEN the toggle TCP CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.shouldShowTotalCookieProtectionCFR = false
        assertFalse(settings.shouldShowTotalCookieProtectionCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tcpShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleTcpShown).joinBlocking()
        assertTrue(settings.shouldShowTotalCookieProtectionCFR)
    }

    @Test
    fun `GIVEN the TCP CFR should be shown WHEN the toggle TCP CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.shouldShowTotalCookieProtectionCFR = true
        assertTrue(settings.shouldShowTotalCookieProtectionCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tcpShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleTcpShown).joinBlocking()
        assertFalse(settings.shouldShowTotalCookieProtectionCFR)
    }

    @Test
    fun `GIVEN the cookie banner blocker CFR should not be shown WHEN the toggle cookie banner blocker CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.shouldShowEraseActionCFR = false
        assertFalse(settings.shouldShowEraseActionCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                cookieBannerBlockerShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleCookieBannerBlockerShown).joinBlocking()
        assertTrue(settings.shouldShowEraseActionCFR)
    }

    @Test
    fun `GIVEN the cookie banner blocker CFR should be shown WHEN the toggle cookie banner blocker CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.shouldShowEraseActionCFR = true
        assertTrue(settings.shouldShowEraseActionCFR)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                cookieBannerBlockerShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleCookieBannerBlockerShown).joinBlocking()
        assertFalse(settings.shouldShowEraseActionCFR)
    }

    @Test
    fun `GIVEN the cookie banners private mode CFR should not be shown WHEN the toggle cookie banners private mode CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.shouldShowCookieBannersCFR = false
        settings.shouldUseCookieBannerPrivateMode = false
        assertFalse(settings.shouldShowCookieBannersCFR)
        assertFalse(settings.shouldUseCookieBannerPrivateMode)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                cookieBannersPrivateModeShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleCookieBannersPrivateModeShown).joinBlocking()
        assertTrue(settings.shouldShowCookieBannersCFR)
        assertTrue(settings.shouldUseCookieBannerPrivateMode)
    }

    @Test
    fun `GIVEN the cookie banners private mode CFR should be shown WHEN the toggle cookie banners private mode CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.shouldShowCookieBannersCFR = true
        settings.shouldUseCookieBannerPrivateMode = true
        assertTrue(settings.shouldShowCookieBannersCFR)
        assertTrue(settings.shouldUseCookieBannerPrivateMode)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                cookieBannersPrivateModeShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleCookieBannersPrivateModeShown).joinBlocking()
        assertFalse(settings.shouldShowCookieBannersCFR)
        assertFalse(settings.shouldUseCookieBannerPrivateMode)
    }

    @Test
    fun `GIVEN the add private tab to home CFR should not be shown WHEN the toggle add private tab to home CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.showedPrivateModeContextualFeatureRecommender = true
        settings.setNumTimesPrivateModeOpened(3)
        assertFalse(settings.shouldShowPrivateModeCfr)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                addPrivateTabToHomeShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleAddPrivateTabToHomeShown).joinBlocking()
        assertTrue(settings.shouldShowPrivateModeCfr)
    }

    @Test
    fun `GIVEN the add private tab to home CFR should be shown WHEN the toggle add private tab to home CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.showedPrivateModeContextualFeatureRecommender = false
        settings.setNumTimesPrivateModeOpened(3)
        assertTrue(settings.shouldShowPrivateModeCfr)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                addPrivateTabToHomeShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleAddPrivateTabToHomeShown).joinBlocking()
        assertFalse(settings.shouldShowPrivateModeCfr)
    }

    @Test
    fun `GIVEN the tab auto close banner CFR should not be shown WHEN the toggle tab auto close banner CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.shouldShowAutoCloseTabsBanner = false
        assertFalse(settings.shouldShowAutoCloseTabsBanner)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tabAutoCloseBannerShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleTabAutoCloseBannerShown).joinBlocking()
        assertTrue(settings.shouldShowAutoCloseTabsBanner)
    }

    @Test
    fun `GIVEN the tab auto close banner CFR should be shown WHEN the toggle tab auto close banner CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.shouldShowAutoCloseTabsBanner = true
        assertTrue(settings.shouldShowAutoCloseTabsBanner)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                tabAutoCloseBannerShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleTabAutoCloseBannerShown).joinBlocking()
        assertFalse(settings.shouldShowAutoCloseTabsBanner)
    }

    @Test
    fun `GIVEN the inactive tabs CFR should not be shown WHEN the toggle inactive tabs CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.shouldShowInactiveTabsOnboardingPopup = false
        assertFalse(settings.shouldShowInactiveTabsOnboardingPopup)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                inactiveTabsShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleInactiveTabsShown).joinBlocking()
        assertTrue(settings.shouldShowInactiveTabsOnboardingPopup)
    }

    @Test
    fun `GIVEN the inactive tabs CFR should be shown WHEN the toggle inactive tabs CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.shouldShowInactiveTabsOnboardingPopup = true
        assertTrue(settings.shouldShowInactiveTabsOnboardingPopup)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                inactiveTabsShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleInactiveTabsShown).joinBlocking()
        assertFalse(settings.shouldShowInactiveTabsOnboardingPopup)
    }

    @Test
    fun `GIVEN the open in app CFR should not be shown WHEN the toggle open in app CFR action is dispatched THEN its preference is set to should be shown`() {
        settings.shouldShowOpenInAppBanner = false
        assertFalse(settings.shouldShowOpenInAppBanner)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                openInAppShown = true,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleOpenInAppShown).joinBlocking()
        assertTrue(settings.shouldShowOpenInAppBanner)
    }

    @Test
    fun `GIVEN the open in app CFR should be shown WHEN the toggle open in app CFR action is dispatched THEN its preference is set to should not be shown`() {
        settings.shouldShowOpenInAppBanner = true
        assertTrue(settings.shouldShowOpenInAppBanner)

        val store = CfrToolsStore(
            initialState = CfrToolsState(
                openInAppShown = false,
            ),
            middlewares = listOf(
                middleware,
            ),
        )
        store.dispatch(CfrToolsAction.ToggleOpenInAppShown).joinBlocking()
        assertFalse(settings.shouldShowOpenInAppBanner)
    }
}
