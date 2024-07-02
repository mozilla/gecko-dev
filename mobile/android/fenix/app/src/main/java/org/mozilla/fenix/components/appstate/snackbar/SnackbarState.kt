/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.snackbar

/**
 * The state of the snackbar to display.
 */
sealed class SnackbarState {
    /**
     * There is no snackbar to display.
     */
    data object None : SnackbarState()

    /**
     * Dismiss an existing snackbar that is displayed with an indefinite duration.
     */
    data object Dismiss : SnackbarState()

    /**
     * There is a translation in progression for the given [sessionId].
     *
     * @property sessionId The ID of the session being translated.
     */
    data class TranslationInProgress(val sessionId: String?) : SnackbarState()
}
