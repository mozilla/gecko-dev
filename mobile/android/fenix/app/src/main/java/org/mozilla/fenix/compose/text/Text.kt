/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.text

import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import androidx.compose.ui.res.stringResource

/**
 * A sealed Type so callers can take advantage of passing resource values without passing resource
 * or context to their mappers, making it easy for them. At the same time, allowing the ability to
 * have string which could be from another source or could be formatted in a feature specific way.
 * This is a base utility that would help all components.
 */
sealed interface Text {

    /**
     * A simple string text.
     *
     * @property value The string value.
     */
    data class String(val value: kotlin.String) : Text

    /**
     * A resource text.
     *
     * @property value The [Int] resource value.
     */
    data class Resource(@StringRes val value: Int) : Text
}

/**
 * Unpacks and returns the value of the text based on the type of [Text].
 */
val Text.value: String
    @Composable
    get() =
        when (this) {
            is Text.String -> this.value
            is Text.Resource -> stringResource(this.value)
        }
