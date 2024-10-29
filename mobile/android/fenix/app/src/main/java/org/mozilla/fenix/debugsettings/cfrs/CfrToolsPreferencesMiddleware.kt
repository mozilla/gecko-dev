/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.debugsettings.cfrs

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

/**
 * [Middleware] that reacts to various [CfrToolsAction]s and updates any corresponding preferences.
 *
 * @param cfrPreferencesRepository [CfrPreferencesRepository] used to access the CFR preferences.
 * @param coroutineScope The coroutine scope used for emitting flows.
 */
class CfrToolsPreferencesMiddleware(
    private val cfrPreferencesRepository: CfrPreferencesRepository,
    private val coroutineScope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : Middleware<CfrToolsState, CfrToolsAction> {

    @Suppress("LongMethod")
    override fun invoke(
        context: MiddlewareContext<CfrToolsState, CfrToolsAction>,
        next: (CfrToolsAction) -> Unit,
        action: CfrToolsAction,
    ) {
        next(action)

        when (action) {
            is CfrToolsAction.Init -> {
                coroutineScope.launch {
                    cfrPreferencesRepository.cfrPreferenceUpdates
                        .collect { cfrPreferenceUpdate ->
                            val updateAction = mapRepoUpdateToStoreAction(cfrPreferenceUpdate)
                            context.store.dispatch(updateAction)
                        }
                }
                cfrPreferencesRepository.init()
            }
            is CfrToolsAction.HomepageSyncShownToggled -> {
                cfrPreferencesRepository.updateCfrPreference(
                    CfrPreferencesRepository.CfrPreferenceUpdate(
                        preferenceType = CfrPreferencesRepository.CfrPreference.HomepageSync,
                        value = context.state.homepageSyncShown,
                    ),
                )
            }
            is CfrToolsAction.HomepageNavToolbarShownToggled -> {
                cfrPreferencesRepository.updateCfrPreference(
                    CfrPreferencesRepository.CfrPreferenceUpdate(
                        preferenceType = CfrPreferencesRepository.CfrPreference.HomepageNavToolbar,
                        value = context.state.homepageNavToolbarShown,
                    ),
                )
            }
            is CfrToolsAction.NavButtonsShownToggled -> {
                cfrPreferencesRepository.updateCfrPreference(
                    CfrPreferencesRepository.CfrPreferenceUpdate(
                        preferenceType = CfrPreferencesRepository.CfrPreference.NavButtons,
                        value = context.state.navButtonsShown,
                    ),
                )
            }
            is CfrToolsAction.AddPrivateTabToHomeShownToggled -> {
                // This will be implemented at a later date due to its complex nature.
                // See https://bugzilla.mozilla.org/show_bug.cgi?id=1916830 for more details.
            }
            is CfrToolsAction.TabAutoCloseBannerShownToggled -> {
                cfrPreferencesRepository.updateCfrPreference(
                    CfrPreferencesRepository.CfrPreferenceUpdate(
                        preferenceType = CfrPreferencesRepository.CfrPreference.TabAutoCloseBanner,
                        value = context.state.tabAutoCloseBannerShown,
                    ),
                )
            }
            is CfrToolsAction.InactiveTabsShownToggled -> {
                cfrPreferencesRepository.updateCfrPreference(
                    CfrPreferencesRepository.CfrPreferenceUpdate(
                        preferenceType = CfrPreferencesRepository.CfrPreference.InactiveTabs,
                        value = context.state.inactiveTabsShown,
                    ),
                )
            }
            is CfrToolsAction.OpenInAppShownToggled -> {
                cfrPreferencesRepository.updateCfrPreference(
                    CfrPreferencesRepository.CfrPreferenceUpdate(
                        preferenceType = CfrPreferencesRepository.CfrPreference.OpenInApp,
                        value = context.state.openInAppShown,
                    ),
                )
            }
            is CfrToolsAction.PwaShownToggled -> {
                // This will be implemented at a later date due to its complex nature.
                // See https://bugzilla.mozilla.org/show_bug.cgi?id=1908225 for more details.
            }
            is CfrToolsAction.ResetLastCFRTimestampButtonClicked -> {
                cfrPreferencesRepository.resetLastCfrTimestamp()
            }
            is CfrToolsAction.CfrPreferenceUpdate -> {} // No-op
        }
    }

    @VisibleForTesting
    internal fun mapRepoUpdateToStoreAction(
        cfrPreferenceUpdate: CfrPreferencesRepository.CfrPreferenceUpdate,
    ): CfrToolsAction {
        return when (cfrPreferenceUpdate.preferenceType) {
            CfrPreferencesRepository.CfrPreference.HomepageSync ->
                CfrToolsAction.HomepageSyncCfrUpdated(newValue = !cfrPreferenceUpdate.value)
            CfrPreferencesRepository.CfrPreference.HomepageNavToolbar ->
                CfrToolsAction.HomepageNavToolbarCfrUpdated(newValue = !cfrPreferenceUpdate.value)
            CfrPreferencesRepository.CfrPreference.NavButtons ->
                CfrToolsAction.NavButtonsCfrUpdated(newValue = !cfrPreferenceUpdate.value)
            CfrPreferencesRepository.CfrPreference.AddPrivateTabToHome ->
                // Note that the new value is not inverted in this CFR because of the different
                // logic for the pref key
                CfrToolsAction.AddPrivateTabToHomeCfrUpdated(newValue = cfrPreferenceUpdate.value)
            CfrPreferencesRepository.CfrPreference.TabAutoCloseBanner ->
                CfrToolsAction.TabAutoCloseBannerCfrUpdated(newValue = !cfrPreferenceUpdate.value)
            CfrPreferencesRepository.CfrPreference.InactiveTabs ->
                CfrToolsAction.InactiveTabsCfrUpdated(newValue = !cfrPreferenceUpdate.value)
            CfrPreferencesRepository.CfrPreference.OpenInApp ->
                CfrToolsAction.OpenInAppCfrUpdated(newValue = !cfrPreferenceUpdate.value)
        }
    }
}
