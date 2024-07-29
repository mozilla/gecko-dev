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
     * @param isError Whether the snackbar should be styled as an error.
     * @param action Optional String resource to display for the action.
     * The [listener] must also be provided to show an action button.
     * @param listener Optional callback to be invoked when the action is clicked.
     * An [action] must also be provided to show an action button.
     */
    fun show(
        @StringRes text: Int,
        duration: Int = LENGTH_ACCESSIBLE,
        isError: Boolean = false,
        @StringRes action: Int = 0,
        listener: ((v: View) -> Unit)? = null,
    ) {
        show(
            snackBarParentView = view,
            text = text,
            duration = duration,
            isError = isError,
            action = action,
            listener = listener,
        )
    }

    /**
     * Displays a snackbar.
     *
     * @param text The text to show.
     * @param duration How long to display the message.
     * @param isError Whether the snackbar should be styled as an error.
     * @param action Optional String to display for the action.
     * The [listener] must also be provided to show an action button.
     * @param listener Optional callback to be invoked when the action is clicked.
     * An [action] must also be provided to show an action button.
     */
    fun show(
        text: String,
        duration: Int = LENGTH_ACCESSIBLE,
        isError: Boolean = false,
        action: String? = null,
        listener: ((v: View) -> Unit)? = null,
    ) = show(
        snackBarParentView = view,
        text = text,
        duration = duration,
        isError = isError,
        action = action,
        listener = listener,
    )

    override fun show(
        snackBarParentView: View,
        @StringRes text: Int,
        duration: Int,
        isError: Boolean,
        @StringRes action: Int,
        listener: ((v: View) -> Unit)?,
    ) = show(
        snackBarParentView = snackBarParentView,
        text = snackBarParentView.context.getString(text),
        duration = duration,
        isError = isError,
        action = if (action == 0) null else snackBarParentView.context.getString(action),
        listener = listener,
    )

    override fun show(
        snackBarParentView: View,
        text: String,
        duration: Int,
        isError: Boolean,
        action: String?,
        listener: ((v: View) -> Unit)?,
    ) {
        val snackbar = FenixSnackbar.make(
            view = snackBarParentView,
            duration = duration,
        ).apply {
            setText(text)
            setAppropriateBackground(isError)
        }

        if (action != null && listener != null) {
            snackbar.setAction(action) {
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
