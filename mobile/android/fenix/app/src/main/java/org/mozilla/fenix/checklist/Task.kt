/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.checklist

import androidx.compose.ui.graphics.painter.Painter

/**
 * A data class representing a task from setup checklist.
 *
 * @param title the task's title.
 * @param icon the icon displayed on the right of the task.
 * @param isCompleted if the task was completed or not.
 * @param iCollapsed if the task is collapsed.
 */
@Suppress("OutdatedDocumentation")
data class Task(
    val title: String,
    val icon: Painter? = null,
    val isCompleted: Boolean? = null,
    val iCollapsed: Boolean = false,
)
