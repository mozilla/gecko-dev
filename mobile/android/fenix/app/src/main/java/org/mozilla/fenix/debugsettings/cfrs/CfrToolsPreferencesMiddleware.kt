/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.cfrs

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.utils.Settings

/**
 * [Middleware] that reacts to various [CfrToolsAction]s and updates any corresponding preferences.
 */
class CfrToolsPreferencesMiddleware(
    private val settings: Settings,
) : Middleware<CfrToolsState, CfrToolsAction> {
    override fun invoke(
        context: MiddlewareContext<CfrToolsState, CfrToolsAction>,
        next: (CfrToolsAction) -> Unit,
        action: CfrToolsAction,
    ) {
        when (action) {
            is CfrToolsAction.ToggleHomepageSyncShown -> {
                settings.showSyncCFR = !settings.showSyncCFR
            }
            is CfrToolsAction.ToggleHomepageNavToolbarShown -> {
                settings.shouldShowNavigationBarCFR = !settings.shouldShowNavigationBarCFR
            }
            is CfrToolsAction.ToggleWallpaperSelectorShown -> {
                settings.showWallpaperOnboarding = !settings.showWallpaperOnboarding
            }
            is CfrToolsAction.ToggleNavButtonsShown -> {
                settings.shouldShowNavigationButtonsCFR = !settings.shouldShowNavigationButtonsCFR
            }
            is CfrToolsAction.ToggleTcpShown -> {
                settings.shouldShowTotalCookieProtectionCFR = !settings.shouldShowTotalCookieProtectionCFR
            }
            is CfrToolsAction.ToggleCookieBannerBlockerShown -> {
                settings.shouldShowEraseActionCFR = !settings.shouldShowEraseActionCFR
            }
            is CfrToolsAction.ToggleCookieBannersPrivateModeShown -> {
                settings.shouldShowCookieBannersCFR = !settings.shouldShowCookieBannersCFR
                settings.shouldUseCookieBannerPrivateMode = !settings.shouldUseCookieBannerPrivateMode
            }
            is CfrToolsAction.ToggleAddPrivateTabToHomeShown -> {
                settings.showedPrivateModeContextualFeatureRecommender =
                    !settings.showedPrivateModeContextualFeatureRecommender
            }
            is CfrToolsAction.ToggleTabAutoCloseBannerShown -> {
                settings.shouldShowAutoCloseTabsBanner = !settings.shouldShowAutoCloseTabsBanner
            }
            is CfrToolsAction.ToggleInactiveTabsShown -> {
                settings.shouldShowInactiveTabsOnboardingPopup = !settings.shouldShowInactiveTabsOnboardingPopup
            }
            is CfrToolsAction.ToggleOpenInAppShown -> {
                settings.shouldShowOpenInAppBanner = !settings.shouldShowOpenInAppBanner
            }
            is CfrToolsAction.TogglePwaShown -> {
                // This will be implemented at a later date due to its complex nature.
                // See https://bugzilla.mozilla.org/show_bug.cgi?id=1908225 for more details.
            }
        }
    }
}
