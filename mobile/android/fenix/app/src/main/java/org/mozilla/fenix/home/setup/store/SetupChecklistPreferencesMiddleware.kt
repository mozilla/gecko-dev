/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.appstate.setup.checklist.ChecklistItem

/**
 * [Middleware] for handling preference updates related to the setup checklist feature.
 */
class SetupChecklistPreferencesMiddleware(private val repository: SetupChecklistRepository) :
    Middleware<AppState, AppAction> {
    override fun invoke(
        context: MiddlewareContext<AppState, AppAction>,
        next: (AppAction) -> Unit,
        action: AppAction,
    ) {
        next(action)

        when (action) {
            is AppAction.SetupChecklistAction.ChecklistItemClicked -> {
                val item = action.item
                if (item is ChecklistItem.Task) {
                    val preference = when (item.type) {
                        ChecklistItem.Task.Type.SELECT_THEME -> PreferenceType.ThemeComplete
                        ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT -> PreferenceType.ToolbarComplete
                        ChecklistItem.Task.Type.EXPLORE_EXTENSION -> PreferenceType.ExtensionsComplete
                        else -> null
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
