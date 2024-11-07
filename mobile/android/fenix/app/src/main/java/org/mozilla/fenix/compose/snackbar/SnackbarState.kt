/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.snackbar

import androidx.compose.material.SnackbarDuration
import org.mozilla.fenix.compose.core.Action
import org.mozilla.fenix.compose.snackbar.SnackbarState.Type

private val defaultDuration = SnackbarDuration.Short
private val defaultType = Type.Default
private val defaultAction: Action? = null
private val defaultOnDismiss: () -> Unit = {}

/**
 * The data to display within a Snackbar.
 *
 * @property message The text to display within a Snackbar.
 * @property duration The duration of the Snackbar.
 * @property type The [Type] used to apply styling.
 * @property action Optional action within the Snackbar.
 * @property onDismiss Invoked when the Snackbar is dismissed.
 */
data class SnackbarState(
    val message: String,
    val duration: SnackbarDuration = defaultDuration,
    val type: Type = defaultType,
    val action: Action? = defaultAction,
    val onDismiss: () -> Unit = defaultOnDismiss,
) {

    /**
     * The type of Snackbar to display.
     */
    enum class Type {
        Default,
        Warning,
    }
}
