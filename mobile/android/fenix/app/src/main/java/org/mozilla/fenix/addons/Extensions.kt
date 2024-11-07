/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.addons

import android.view.View
import androidx.compose.material.SnackbarDuration
import org.mozilla.fenix.compose.snackbar.Snackbar
import org.mozilla.fenix.compose.snackbar.SnackbarState

/**
 * Shows the Fenix Snackbar in the given view along with the provided text.
 *
 * @param view A [View] used to determine a parent for the [Snackbar].
 * @param text The text to display in the [Snackbar].
 * @param duration The duration to show the [Snackbar] for.
 */
internal fun showSnackBar(view: View, text: String, duration: SnackbarDuration = SnackbarDuration.Short) {
    Snackbar.make(
        snackBarParentView = view,
        snackbarState = SnackbarState(
            message = text,
            duration = duration,
        ),
    ).show()
}
