/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.ext

import androidx.compose.ui.text.intl.Locale
import java.text.NumberFormat
import java.util.Locale.Builder

/**
 * Returns a localized string representation of the value.
 */
fun Int.toLocaleString(): String =
    NumberFormat.getNumberInstance(
        Builder().setLanguage(Locale.current.language).build(),
    ).format(this)
