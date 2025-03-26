/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.setup.checklist.ChecklistItem
import org.mozilla.fenix.components.appstate.setup.checklist.SetupChecklistState

/**
 * [Middleware] for handling side effects of the setup checklist feature.
 */
class SetupChecklistMiddleware(
    private val triggerDefaultPrompt: () -> Unit,
    private val navigateToSignIn: () -> Unit,
    private val navigateToCustomize: () -> Unit,
    private val navigateToExtensions: () -> Unit,
    private val installSearchWidget: () -> Unit,
) : Middleware<SetupChecklistState, AppAction.SetupChecklistAction> {
    override fun invoke(
        context: MiddlewareContext<SetupChecklistState, AppAction.SetupChecklistAction>,
        next: (AppAction.SetupChecklistAction) -> Unit,
        action: AppAction.SetupChecklistAction,
    ) {
        next(action)

        when (action) {
            is AppAction.SetupChecklistAction.Closed -> {}
            is AppAction.SetupChecklistAction.ChecklistItemClicked -> {
                if (action.item is ChecklistItem.Task) {
                    when (action.item.type) {
                        ChecklistItem.Task.Type.SET_AS_DEFAULT -> triggerDefaultPrompt()
                        ChecklistItem.Task.Type.SIGN_IN -> navigateToSignIn()
                        ChecklistItem.Task.Type.SELECT_THEME,
                        ChecklistItem.Task.Type.CHANGE_TOOLBAR_PLACEMENT,
                        -> navigateToCustomize()
                        ChecklistItem.Task.Type.INSTALL_SEARCH_WIDGET -> installSearchWidget()
                        ChecklistItem.Task.Type.EXPLORE_EXTENSION -> navigateToExtensions()
                    }
                }
            }
        }
    }
}
