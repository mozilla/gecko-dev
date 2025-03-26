/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.setup.checklist

import mozilla.components.lib.state.State

/**
 * Represents the [State] of the setup checklist feature.
 *
 * @property checklistItems The list of checklist items.
 * @property progress The progress of the current checklist.
 */
data class SetupChecklistState(
    val checklistItems: List<ChecklistItem> = emptyList(),
    val progress: Progress = checklistItems.getTaskProgress(),
) : State
