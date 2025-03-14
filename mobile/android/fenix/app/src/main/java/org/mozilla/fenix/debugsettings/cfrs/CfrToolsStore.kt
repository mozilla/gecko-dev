/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.cfrs

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.UiStore

/**
 * Value type that represents the state of the CFR Tools.
 *
 * @property homepageSyncShown Whether the homepage sync CFR has been shown.
 * @property homepageNavToolbarShown Whether the homepage navigation toolbar CFR has been shown.
 * @property homepageSearchBarShown Whether the homepage search toolbar CFR has been shown.
 * @property navButtonsShown Whether the navigation buttons CFR has been shown.
 * @property addPrivateTabToHomeShown Whether the add private tab to home CFR has been shown.
 * @property tabAutoCloseBannerShown Whether the tab auto close banner CFR has been shown.
 * @property inactiveTabsShown Whether the inactive tabs CFR has been shown.
 * @property openInAppShown Whether the open in app CFR has been shown.
 * @property pwaShown Whether the progressive web app dialog CFR has been shown.
 */
data class CfrToolsState(
    val homepageSyncShown: Boolean = false,
    val homepageNavToolbarShown: Boolean = false,
    val homepageSearchBarShown: Boolean = false,
    val navButtonsShown: Boolean = false,
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
     * Dispatched when the store is initialized.
     */
    data object Init : CfrToolsAction()

    /**
     * Toggle whether the homepage sync CFR has been shown.
     */
    data object HomepageSyncShownToggled : CfrToolsAction()

    /**
     * Toggle whether the homepage navigation toolbar CFR has been shown.
     */
    data object HomepageNavToolbarShownToggled : CfrToolsAction()

    /**
     * Toggle whether the homepage searchbar CFR has been shown.
     */
    data object HomepageSearchBarShownToggled : CfrToolsAction()

    /**
     * Toggle whether the navigation buttons CFR has been shown.
     */
    data object NavButtonsShownToggled : CfrToolsAction()

    /**
     * Toggle whether the add private tab to home (private mode) CFR has been shown.
     */
    data object AddPrivateTabToHomeShownToggled : CfrToolsAction()

    /**
     * Toggle whether the tab auto close banner CFR has been shown.
     */
    data object TabAutoCloseBannerShownToggled : CfrToolsAction()

    /**
     * Toggle whether the inactive tabs CFR has been shown.
     */
    data object InactiveTabsShownToggled : CfrToolsAction()

    /**
     * Toggle whether the open in app CFR has been shown.
     */
    data object OpenInAppShownToggled : CfrToolsAction()

    /**
     * Toggle whether the progressive web app dialog CFR has been shown.
     */
    data object PwaShownToggled : CfrToolsAction()

    /**
     * Reset lastCfrShownTimeInMillis to 0.
     */
    data object ResetLastCFRTimestampButtonClicked : CfrToolsAction()

    /**
     * [Action] fired when the user toggles a CFR.
     */
    sealed interface CfrPreferenceUpdate

    /**
     * [CfrPreferenceUpdate] fired when the user toggles the homepage sync CFR.
     *
     * @property newValue The updated value of the pref indicating whether or not to show the homepage
     * sync CFR.
     */
    data class HomepageSyncCfrUpdated(val newValue: Boolean) : CfrToolsAction(), CfrPreferenceUpdate

    /**
     * [CfrPreferenceUpdate] fired when the user toggles the homepage nav toolbar CFR.
     *
     * @property newValue The updated value of the pref indicating whether or not to show the homepage
     * nav toolbar CFR.
     */
    data class HomepageNavToolbarCfrUpdated(val newValue: Boolean) : CfrToolsAction(), CfrPreferenceUpdate

    /**
     * [CfrPreferenceUpdate] fired when the user toggles the homepage searchbar CFR.
     *
     * @property newValue The updated value of the pref indicating whether or not to show the homepage
     * searchbar CFR.
     */
    data class HomepageSearchbarCfrUpdated(val newValue: Boolean) : CfrToolsAction(), CfrPreferenceUpdate

    /**
     * [CfrPreferenceUpdate] fired when the user toggles the nav buttons CFR.
     *
     * @property newValue The updated value of the pref indicating whether or not to show the nav
     * buttons CFR.
     */
    data class NavButtonsCfrUpdated(val newValue: Boolean) : CfrToolsAction(), CfrPreferenceUpdate

    /**
     * [CfrPreferenceUpdate] fired when the user toggles the add private tab to home CFR.
     *
     * @property newValue The updated value of the pref indicating whether or not to show the add
     * private tab to home CFR.
     */
    data class AddPrivateTabToHomeCfrUpdated(val newValue: Boolean) : CfrToolsAction(), CfrPreferenceUpdate

    /**
     * [CfrPreferenceUpdate] fired when the user toggles the tab auto close banner CFR.
     *
     * @property newValue The updated value of the pref indicating whether or not to show the tab auto
     * close banner CFR.
     */
    data class TabAutoCloseBannerCfrUpdated(val newValue: Boolean) : CfrToolsAction(), CfrPreferenceUpdate

    /**
     * [CfrPreferenceUpdate] fired when the user toggles the inactive tabs CFR.
     *
     * @property newValue The updated value of the pref indicating whether or not to show the inactive
     * tabs CFR.
     */
    data class InactiveTabsCfrUpdated(val newValue: Boolean) : CfrToolsAction(), CfrPreferenceUpdate

    /**
     * [CfrPreferenceUpdate] fired when the user toggles the open in app CFR.
     *
     * @property newValue The updated value of the pref indicating whether or not to show the open in
     * app CFR.
     */
    data class OpenInAppCfrUpdated(val newValue: Boolean) : CfrToolsAction(), CfrPreferenceUpdate

    /**
     * [CfrPreferenceUpdate] fired when the user toggles the PWA CFR.
     *
     * @property newValue The updated value of the pref indicating whether or not to show PWA CFR.
     */
    data class PwaCfrUpdated(val newValue: Boolean) : CfrToolsAction(), CfrPreferenceUpdate
}

/**
 * Reducer for [CfrToolsStore].
 */
internal object CfrToolsReducer {
    @Suppress("ComplexMethod")
    fun reduce(state: CfrToolsState, action: CfrToolsAction): CfrToolsState {
        return when (action) {
            is CfrToolsAction.Init -> state
            is CfrToolsAction.HomepageSyncShownToggled ->
                state.copy(homepageSyncShown = !state.homepageSyncShown)
            is CfrToolsAction.HomepageNavToolbarShownToggled ->
                state.copy(homepageNavToolbarShown = !state.homepageNavToolbarShown)
            is CfrToolsAction.HomepageSearchBarShownToggled ->
                state.copy(homepageSearchBarShown = !state.homepageSearchBarShown)
            is CfrToolsAction.NavButtonsShownToggled ->
                state.copy(navButtonsShown = !state.navButtonsShown)
            is CfrToolsAction.AddPrivateTabToHomeShownToggled ->
                state.copy(addPrivateTabToHomeShown = !state.addPrivateTabToHomeShown)
            is CfrToolsAction.TabAutoCloseBannerShownToggled ->
                state.copy(tabAutoCloseBannerShown = !state.tabAutoCloseBannerShown)
            is CfrToolsAction.InactiveTabsShownToggled ->
                state.copy(inactiveTabsShown = !state.inactiveTabsShown)
            is CfrToolsAction.OpenInAppShownToggled ->
                state.copy(openInAppShown = !state.openInAppShown)
            is CfrToolsAction.PwaShownToggled ->
                state.copy(pwaShown = !state.pwaShown)
            is CfrToolsAction.ResetLastCFRTimestampButtonClicked -> state
            is CfrToolsAction.AddPrivateTabToHomeCfrUpdated ->
                state.copy(addPrivateTabToHomeShown = action.newValue)
            is CfrToolsAction.HomepageNavToolbarCfrUpdated ->
                state.copy(homepageNavToolbarShown = action.newValue)
            is CfrToolsAction.HomepageSearchbarCfrUpdated ->
                state.copy(homepageSearchBarShown = action.newValue)
            is CfrToolsAction.HomepageSyncCfrUpdated ->
                state.copy(homepageSyncShown = action.newValue)
            is CfrToolsAction.InactiveTabsCfrUpdated ->
                state.copy(inactiveTabsShown = action.newValue)
            is CfrToolsAction.NavButtonsCfrUpdated ->
                state.copy(navButtonsShown = action.newValue)
            is CfrToolsAction.OpenInAppCfrUpdated ->
                state.copy(openInAppShown = action.newValue)
            is CfrToolsAction.PwaCfrUpdated ->
                state.copy(pwaShown = action.newValue)
            is CfrToolsAction.TabAutoCloseBannerCfrUpdated ->
                state.copy(tabAutoCloseBannerShown = action.newValue)
        }
    }
}

/**
 * A [UiStore] that holds the [CfrToolsState] for the CFR Tools and reduces [CfrToolsAction]s
 * dispatched to the store.
 */
class CfrToolsStore(
    initialState: CfrToolsState = CfrToolsState(),
    middlewares: List<Middleware<CfrToolsState, CfrToolsAction>> = emptyList(),
) : UiStore<CfrToolsState, CfrToolsAction>(
    initialState,
    CfrToolsReducer::reduce,
    middlewares,
) {
    init {
        dispatch(CfrToolsAction.Init)
    }
}
