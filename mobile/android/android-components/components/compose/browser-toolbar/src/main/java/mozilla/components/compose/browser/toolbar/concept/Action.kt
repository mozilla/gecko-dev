/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.concept

import androidx.annotation.ColorInt
import androidx.annotation.DrawableRes
import androidx.compose.runtime.Composable

/**
 * Actions that can be added to the toolbar.
 */
sealed class Action {

    /**
     * An action button to be added to the toolbar.
     *
     * @property icon The icon resource to be displayed for the action button.
     * @property contentDescription The content description for the action button.
     * @property tint The color resource used to tint the icon.
     * @property onClick Invoked when the action button is clicked.
     */
    data class ActionButton(
        @DrawableRes val icon: Int,
        val contentDescription: String?,
        @ColorInt val tint: Int,
        val onClick: () -> Unit,
    ) : Action()

    /**
     * A custom action that can be displayed with the provided Composable [content].
     *
     * @param content Composable content to display.
     */
    data class CustomAction(
        val content: @Composable () -> Unit,
    ) : Action()
}
