/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.snackbar

import androidx.compose.material.SnackbarDuration
import com.google.android.material.snackbar.Snackbar.LENGTH_INDEFINITE
import com.google.android.material.snackbar.Snackbar.LENGTH_LONG
import com.google.android.material.snackbar.Snackbar.LENGTH_SHORT

/**
 * Helper function to convert a [SnackbarDuration] to a Material Integer constant.
 */
fun SnackbarDuration.toIntegerSnackbarDuration(): Int = when (this) {
    SnackbarDuration.Short -> LENGTH_SHORT
    SnackbarDuration.Long -> LENGTH_LONG
    SnackbarDuration.Indefinite -> LENGTH_INDEFINITE
}

/**
 * Helper function to convert a Material Integer constant to a [SnackbarDuration].
 */
fun Int.toSnackbarDuration(): SnackbarDuration = when (this) {
    LENGTH_SHORT -> SnackbarDuration.Short
    LENGTH_LONG -> SnackbarDuration.Long
    LENGTH_INDEFINITE -> SnackbarDuration.Indefinite
    else -> SnackbarDuration.Short
}
