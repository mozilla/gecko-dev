/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.concept

import android.graphics.drawable.Drawable
import androidx.annotation.ColorInt
import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.compose.runtime.Composable
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu

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

    /**
     * An action button styled as a dropdown button to be added to the toolbar.
     * This wraps the provided [icon] at the start with a down arrow to it's right to indicate that
     * clicking this will open a dropdown menu.
     *
     * @property icon The icon for this button.
     * @property contentDescription The content description for this button.
     * @property menu The [BrowserToolbarMenu] to show when this button is clicked.
     */
    data class DropdownAction(
        val icon: Drawable,
        @StringRes val contentDescription: Int,
        val menu: BrowserToolbarMenu,
    ) : Action()
}
