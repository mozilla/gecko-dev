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
            is CfrToolsAction.HomepageSearchBarShownToggled -> {
                cfrPreferencesRepository.updateCfrPreference(
                    CfrPreferencesRepository.CfrPreferenceUpdate(
                        preferenceType = CfrPreferencesRepository.CfrPreference.HomepageSearchBar,
                        value = context.state.homepageSearchBarShown,
                    ),
                )
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
            is CfrToolsAction.LoadCfrPreference -> {} // No-op
        }
    }

    @VisibleForTesting
    internal fun mapRepoUpdateToStoreAction(
        cfrPreferenceUpdate: CfrPreferencesRepository.CfrPreferenceUpdate,
    ): CfrToolsAction {
        return when (cfrPreferenceUpdate.preferenceType) {
            CfrPreferencesRepository.CfrPreference.HomepageSearchBar ->
                CfrToolsAction.HomepageSearchbarCfrLoaded(newValue = !cfrPreferenceUpdate.value)
            CfrPreferencesRepository.CfrPreference.TabAutoCloseBanner ->
                CfrToolsAction.TabAutoCloseBannerCfrLoaded(newValue = !cfrPreferenceUpdate.value)
            CfrPreferencesRepository.CfrPreference.InactiveTabs ->
                CfrToolsAction.InactiveTabsCfrLoaded(newValue = !cfrPreferenceUpdate.value)
            CfrPreferencesRepository.CfrPreference.OpenInApp ->
                CfrToolsAction.OpenInAppCfrLoaded(newValue = !cfrPreferenceUpdate.value)
        }
    }
}
