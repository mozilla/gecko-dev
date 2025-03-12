/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext

// todo complete as part of https://bugzilla.mozilla.org/show_bug.cgi?id=1951909

/**
 * [Middleware] for recording telemetry related to the setup checklist feature.
 */
class SetupChecklistTelemetryMiddleware(val telemetry: SetupChecklistTelemetryRecorder) :
    Middleware<SetupChecklistState, SetupChecklistAction> {
    override fun invoke(
        context: MiddlewareContext<SetupChecklistState, SetupChecklistAction>,
        next: (SetupChecklistAction) -> Unit,
        action: SetupChecklistAction,
    ) {
        next(action)

        when (action) {
            is SetupChecklistAction.Init -> {}
            is SetupChecklistAction.Closed -> {}
            is SetupChecklistAction.DefaultBrowserClicked -> {}
            is SetupChecklistAction.SyncClicked -> {}
            is SetupChecklistAction.ThemeSelectionClicked -> {}
            is SetupChecklistAction.ToolbarSelectionClicked -> {}
            is SetupChecklistAction.ExtensionsClicked -> {}
            is SetupChecklistAction.AddSearchWidgetClicked -> {}
            is SetupChecklistAction.ViewState -> {}
        }
    }
}

/**
 * Interface for recording telemetry related to the setup checklist feature.
 */
interface SetupChecklistTelemetryRecorder {
    /**
     * Called when a task in the setup checklist is clicked.
     */
    fun taskClicked(action: SetupChecklistAction)
}
