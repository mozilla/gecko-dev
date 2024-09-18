/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.cfrs

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.lib.state.UiStore

/**
 * Value type that represents the state of the CFR Tools.
 *
 * @property homepageSyncShown Whether the homepage sync CFR has been shown.
 * @property homepageNavToolbarShown Whether the homepage navigation toolbar CFR has been shown.
 * @property wallpaperSelectorShown Whether the wallpaper selector CFR has been shown.
 * @property navButtonsShown Whether the navigation buttons CFR has been shown.
 * @property tcpShown Whether the total cookies protection CFR has been shown.
 * @property cookieBannerBlockerShown Whether the cookie banner blocker CFR has been shown.
 * @property cookieBannersPrivateModeShown Whether the cookie banners private mode CFR has been shown.
 * @property addPrivateTabToHomeShown Whether the add private tab to home CFR has been shown.
 * @property tabAutoCloseBannerShown Whether the tab auto close banner CFR has been shown.
 * @property inactiveTabsShown Whether the inactive tabs CFR has been shown.
 * @property openInAppShown Whether the open in app CFR has been shown.
 * @property pwaShown Whether the progressive web app dialog CFR has been shown.
 */
data class CfrToolsState(
    val homepageSyncShown: Boolean = false,
    val homepageNavToolbarShown: Boolean = false,
    val wallpaperSelectorShown: Boolean = false,
    val navButtonsShown: Boolean = false,
    val tcpShown: Boolean = false,
    val cookieBannerBlockerShown: Boolean = false,
    val cookieBannersPrivateModeShown: Boolean = false,
    val addPrivateTabToHomeShown: Boolean = false,
    val tabAutoCloseBannerShown: Boolean = false,
    val inactiveTabsShown: Boolean = false,
    val openInAppShown: Boolean = false,
    val pwaShown: Boolean = false,
) : State

/**
 * [Action] implementation related to [CfrToolsStore].
 */
sealed class CfrToolsAction : Action {

    /**
     * Toggle whether the homepage sync CFR has been shown.
     */
    object ToggleHomepageSyncShown : CfrToolsAction()

    /**
     * Toggle whether the homepage navigation toolbar CFR has been shown.
     */
    object ToggleHomepageNavToolbarShown : CfrToolsAction()

    /**
     * Toggle whether the wallpaper selector CFR has been shown.
     */
    object ToggleWallpaperSelectorShown : CfrToolsAction()

    /**
     * Toggle whether the navigation buttons CFR has been shown.
     */
    object ToggleNavButtonsShown : CfrToolsAction()

    /**
     * Toggle whether the total cookies protection CFR has been shown.
     */
    object ToggleTcpShown : CfrToolsAction()

    /**
     * Toggle whether the cookie banner blocker (erase action) CFR has been shown.
     */
    object ToggleCookieBannerBlockerShown : CfrToolsAction()

    /**
     * Toggle whether the cookie banners private mode CFR has been shown.
     */
    object ToggleCookieBannersPrivateModeShown : CfrToolsAction()

    /**
     * Toggle whether the add private tab to home (private mode) CFR has been shown.
     */
    object ToggleAddPrivateTabToHomeShown : CfrToolsAction()

    /**
     * Toggle whether the tab auto close banner CFR has been shown.
     */
    object ToggleTabAutoCloseBannerShown : CfrToolsAction()

    /**
     * Toggle whether the inactive tabs CFR has been shown.
     */
    object ToggleInactiveTabsShown : CfrToolsAction()

    /**
     * Toggle whether the open in app CFR has been shown.
     */
    object ToggleOpenInAppShown : CfrToolsAction()

    /**
     * Toggle whether the progressive web app dialog CFR has been shown.
     */
    object TogglePwaShown : CfrToolsAction()
}

/**
 * Reducer for [CfrToolsStore].
 */
internal object CfrToolsReducer {
    fun reduce(state: CfrToolsState, action: CfrToolsAction): CfrToolsState {
        return when (action) {
            is CfrToolsAction.ToggleHomepageSyncShown ->
                state.copy(homepageSyncShown = !state.homepageSyncShown)
            is CfrToolsAction.ToggleHomepageNavToolbarShown ->
                state.copy(homepageNavToolbarShown = !state.homepageNavToolbarShown)
            is CfrToolsAction.ToggleWallpaperSelectorShown ->
                state.copy(wallpaperSelectorShown = !state.wallpaperSelectorShown)
            is CfrToolsAction.ToggleNavButtonsShown ->
                state.copy(navButtonsShown = !state.navButtonsShown)
            is CfrToolsAction.ToggleTcpShown ->
                state.copy(tcpShown = !state.tcpShown)
            is CfrToolsAction.ToggleCookieBannerBlockerShown ->
                state.copy(cookieBannerBlockerShown = !state.cookieBannerBlockerShown)
            is CfrToolsAction.ToggleCookieBannersPrivateModeShown ->
                state.copy(cookieBannersPrivateModeShown = !state.cookieBannersPrivateModeShown)
            is CfrToolsAction.ToggleAddPrivateTabToHomeShown ->
                state.copy(addPrivateTabToHomeShown = !state.addPrivateTabToHomeShown)
            is CfrToolsAction.ToggleTabAutoCloseBannerShown ->
                state.copy(tabAutoCloseBannerShown = !state.tabAutoCloseBannerShown)
            is CfrToolsAction.ToggleInactiveTabsShown ->
                state.copy(inactiveTabsShown = !state.inactiveTabsShown)
            is CfrToolsAction.ToggleOpenInAppShown ->
                state.copy(openInAppShown = !state.openInAppShown)
            is CfrToolsAction.TogglePwaShown ->
                state.copy(pwaShown = !state.pwaShown)
        }
    }
}

/**
 * A [Store] that holds the [CfrToolsState] for the CFR Tools and reduces [CfrToolsAction]s
 * dispatched to the store.
 */
class CfrToolsStore(
    initialState: CfrToolsState = CfrToolsState(),
    middlewares: List<Middleware<CfrToolsState, CfrToolsAction>> = emptyList(),
) : UiStore<CfrToolsState, CfrToolsAction>(
    initialState,
    CfrToolsReducer::reduce,
    middlewares,
)
