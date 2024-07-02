/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.snackbar

import android.view.View
import androidx.annotation.StringRes
import mozilla.components.ui.widgets.SnackbarDelegate
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.components.FenixSnackbar.Companion.LENGTH_ACCESSIBLE

/**
 * An implementation of [SnackbarDelegate] used to display the snackbar.
 */
class FenixSnackbarDelegate(private val view: View) : SnackbarDelegate {

    // Holds onto a reference of a [FenixSnackbar] that is displayed for an indefinite duration.
    private var snackbar: FenixSnackbar? = null

    /**
     * Displays a snackbar.
     *
     * @param text The text to show. Can be formatted text.
     * @param duration How long to display the message.
     * @param action String resource to display for the action.
     * @param listener callback to be invoked when the action is clicked.
     */
    fun show(
        @StringRes text: Int,
        duration: Int = LENGTH_ACCESSIBLE,
        @StringRes action: Int = 0,
        listener: ((v: View) -> Unit)? = null,
    ) {
        show(
            snackBarParentView = view,
            text = text,
            duration = duration,
            action = action,
            listener = listener,
        )
    }

    override fun show(
        snackBarParentView: View,
        @StringRes text: Int,
        duration: Int,
        @StringRes action: Int,
        listener: ((v: View) -> Unit)?,
    ) {
        val snackbar = FenixSnackbar.make(
            view = snackBarParentView,
            duration = duration,
            isDisplayedWithBrowserToolbar = true,
        )
            .setText(snackBarParentView.context.getString(text))

        if (action != 0 && listener != null) {
            snackbar.setAction(snackBarParentView.context.getString(action)) {
                listener.invoke(
                    snackBarParentView,
                )
            }
        }

        if (duration == FenixSnackbar.LENGTH_INDEFINITE) {
            // Dismiss any existing snackbar with an indefinite duration.
            this.snackbar?.dismiss()
            this.snackbar = snackbar
        }

        snackbar.show()
    }

    /**
     * Dismiss the existing snackbar.
     */
    fun dismiss() {
        snackbar?.dismiss()
    }
}
