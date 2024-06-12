/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser

import android.view.View
import androidx.annotation.StringRes
import mozilla.components.ui.widgets.SnackbarDelegate
import org.mozilla.fenix.components.FenixSnackbar

/**
 * An implementation of [SnackbarDelegate] used to override the default snackbar behavior for
 * showing a snackbar from a context menu item.
 */
class ContextMenuSnackbarDelegate(private val view: View) : SnackbarDelegate {

    override fun show(
        snackBarParentView: View,
        @StringRes text: Int,
        duration: Int,
        @StringRes action: Int,
        listener: ((v: View) -> Unit)?,
    ) {
        val snackbar = FenixSnackbar.make(
            view = view,
            duration = FenixSnackbar.LENGTH_SHORT,
            isDisplayedWithBrowserToolbar = true,
        )
            .setText(view.context.getString(text))

        if (action != 0 && listener != null) {
            snackbar.setAction(view.context.getString(action)) { listener.invoke(view) }
        }

        snackbar.show()
    }
}
