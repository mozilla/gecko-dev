/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.view.View
import androidx.annotation.StringRes
import mozilla.components.ui.widgets.SnackbarDelegate
import org.mozilla.fenix.compose.core.Action
import org.mozilla.fenix.compose.snackbar.Snackbar
import org.mozilla.fenix.compose.snackbar.SnackbarState

/**
 * An implementation of [SnackbarDelegate] used to override the default snackbar behavior for
 * showing a snackbar from a context menu item.
 */
class ContextMenuSnackbarDelegate : SnackbarDelegate {

    override fun show(
        snackBarParentView: View,
        @StringRes text: Int,
        duration: Int,
        isError: Boolean,
        @StringRes action: Int,
        listener: ((v: View) -> Unit)?,
    ) = show(
        snackBarParentView,
        text = snackBarParentView.context.getString(text),
        duration = duration,
        action = when (action != 0 && listener != null) {
            true -> snackBarParentView.context.getString(action)
            else -> null
        },
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
        val snackbarAction: Action? = if (action != null && listener != null) {
            Action(
                label = action,
                onClick = {
                    listener.invoke(snackBarParentView)
                },
            )
        } else {
            null
        }

        Snackbar.make(
            snackBarParentView = snackBarParentView,
            snackbarState = SnackbarState(
                message = text,
                duration = SnackbarState.Duration.Preset.Short,
                action = snackbarAction,
            ),
        ).show()
    }
}
