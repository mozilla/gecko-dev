/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.setup.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.MiddlewareContext
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.checklist.ChecklistItem

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
            is SetupChecklistAction.ChecklistItemClicked -> {
                if (action.item is ChecklistItem.Task) {
                    telemetry.taskClicked(action.item)
                }
            }
            else -> {
                // no-op
            }
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
    fun taskClicked(task: ChecklistItem.Task)
}

/**
 * Default implementation for recording telemetry related to the setup checklist feature.
 */
class DefaultSetupChecklistTelemetryRecorder : SetupChecklistTelemetryRecorder {
    /**
     * Records the SetupChecklist item telemetry based on [task].
     *
     * @param task ChecklistItem task that was clicked.
     */
    override fun taskClicked(task: ChecklistItem.Task) {
        Onboarding.setupChecklistTaskClicked.record(
            Onboarding.SetupChecklistTaskClickedExtra(task.type.telemetryName),
        )
    }
}
