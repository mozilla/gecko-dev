/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.setup.checklist.ChecklistItem

/**
 * [Middleware] for handling preference updates related to the setup checklist feature.
 *
 * @param repository the [SetupChecklistRepository] used to access the setup checklist preferences.
 * @param coroutineScope the coroutine scope used for emitting flows.
 */
class SetupChecklistPreferencesMiddleware(
    private val repository: SetupChecklistRepository,
    private val coroutineScope: CoroutineScope = CoroutineScope(Dispatchers.Main),
) : Middleware<AppState, AppAction> {

    override fun invoke(
        context: MiddlewareContext<AppState, AppAction>,
        next: (AppAction) -> Unit,
        action: AppAction,
    ) {
        next(action)

        when (action) {
            is AppAction.SetupChecklistAction.Init -> {
                coroutineScope.launch {
                    repository.setupChecklistPreferenceUpdates
                        .collect { preferenceUpdate ->
                            val updateAction = mapRepoUpdateToStoreAction(preferenceUpdate)
                            context.store.dispatch(updateAction)
                        }
                }
                repository.init()
            }

            is AppAction.SetupChecklistAction.ChecklistItemClicked -> {
                val item = action.item
                if (item is ChecklistItem.Task) {
                    val preference = when (item.type) {
                        ChecklistItem.Task.Type.SELECT_THEME -> SetupChecklistPreference.ThemeComplete
                        ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT -> SetupChecklistPreference.ToolbarComplete
                        ChecklistItem.Task.Type.EXPLORE_EXTENSION -> SetupChecklistPreference.ExtensionsComplete

                        // no-ops
                        // these preferences are handled elsewhere outside of the setup checklist feature.
                        ChecklistItem.Task.Type.SET_AS_DEFAULT,
                        ChecklistItem.Task.Type.SIGN_IN,
                        ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET,
                        -> null
                    }

                    preference?.let { repository.setPreference(it, true) }
                }
            }

            else -> {
                // no-op
            }
        }
    }
}

@VisibleForTesting
internal fun mapRepoUpdateToStoreAction(
    preferenceUpdate: SetupChecklistRepository.SetupChecklistPreferenceUpdate,
) = when (preferenceUpdate.preference) {
    SetupChecklistPreference.SetToDefault -> ChecklistItem.Task.Type.SET_AS_DEFAULT
    SetupChecklistPreference.SignIn -> ChecklistItem.Task.Type.SIGN_IN
    SetupChecklistPreference.ThemeComplete -> ChecklistItem.Task.Type.SELECT_THEME
    SetupChecklistPreference.ToolbarComplete -> ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT
    SetupChecklistPreference.ExtensionsComplete -> ChecklistItem.Task.Type.EXPLORE_EXTENSION
    SetupChecklistPreference.InstallSearchWidget -> ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET
}.run {
    AppAction.SetupChecklistAction.TaskPreferenceUpdated(
        this,
        preferenceUpdate.value,
    )
}
