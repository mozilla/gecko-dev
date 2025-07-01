/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.snackbar

import androidx.compose.material.SnackbarHostState
import androidx.compose.material.SnackbarResult
import org.mozilla.fenix.compose.snackbar.SnackbarState.Type

/**
 * State of the [SnackbarHost], controls the queue and the current [Snackbar] being shown inside
 * the [SnackbarHost].
 *
 * @property defaultSnackbarHostState Underlying [SnackbarHostState] used to display Snackbars with
 * the [Type.Default] style.
 * @property warningSnackbarHostState Underlying [SnackbarHostState] used to display Snackbars with
 *  * the [Type.Warning] style.
 */
class AcornSnackbarHostState(
    val defaultSnackbarHostState: SnackbarHostState = SnackbarHostState(),
    val warningSnackbarHostState: SnackbarHostState = SnackbarHostState(),
) {

    /**
     * Request a Snackbar to be displayed with the provided data.
     *
     * @param snackbarState The [SnackbarState] to be displayed.
     */
    suspend fun showSnackbar(snackbarState: SnackbarState) {
        val result = when (snackbarState.type) {
            Type.Default -> {
                defaultSnackbarHostState.showSnackbar(
                    message = snackbarState.message,
                    actionLabel = snackbarState.action?.label,
                    duration = snackbarState.toSnackbarDuration(),
                )
            }

            Type.Warning -> {
                warningSnackbarHostState.showSnackbar(
                    message = snackbarState.message,
                    actionLabel = snackbarState.action?.label,
                    duration = snackbarState.toSnackbarDuration(),
                )
            }
        }

        when (result) {
            SnackbarResult.Dismissed -> {
                snackbarState.onDismiss()
            }

            SnackbarResult.ActionPerformed -> {
                snackbarState.action?.onClick?.invoke()
            }
        }
    }
}
