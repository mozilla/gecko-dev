/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme

private val MENU_ITEM_HEIGHT_WITHOUT_DESC = 52.dp

private val MENU_ITEM_HEIGHT_WITH_DESC = 56.dp

/**
 * An [IconListItem] wrapper for menu items in a [MenuGroup] with an optional icon at the end.
 *
 * @param label The label in the menu item.
 * @param beforeIconPainter [Painter] used to display an [Icon] before the list item.
 * @param beforeIconDescription Content description of the icon.
 * @param description An optional description text below the label.
 * @param state The state of the menu item to display.
 * @param descriptionState The state of menu item description to display.
 * @param onClick Invoked when the user clicks on the item.
 * @param showDivider Whether or not to display a vertical divider line before the [IconButton]
 * at the end.
 * @param afterIconPainter [Painter] used to display an [IconButton] after the list item.
 * @param afterIconDescription Content description of the icon.
 * @param onAfterIconClick Invoked when the user clicks on the icon. An [IconButton] will be
 * displayed if this is provided. Otherwise, an [Icon] will be displayed.
 */
@Composable
internal fun MenuItem(
    label: String,
    beforeIconPainter: Painter,
    beforeIconDescription: String? = null,
    description: String? = null,
    state: MenuItemState = MenuItemState.ENABLED,
    descriptionState: MenuItemState = MenuItemState.ENABLED,
    onClick: (() -> Unit)? = null,
    showDivider: Boolean = false,
    afterIconPainter: Painter? = null,
    afterIconDescription: String? = null,
    onAfterIconClick: (() -> Unit)? = null,
) {
    val labelTextColor = getLabelTextColor(state = state)
    val descriptionTextColor = getDescriptionTextColor(state = descriptionState)
    val iconTint = getIconTint(state = state)
    val enabled = state != MenuItemState.DISABLED

    IconListItem(
        label = label,
        modifier = Modifier
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = LocalIndication.current,
                enabled = enabled,
            ) { onClick?.invoke() }
            .clearAndSetSemantics {
                role = Role.Button
                this.contentDescription = label
            }
            .wrapContentSize(),
        labelTextColor = labelTextColor,
        maxLabelLines = 2,
        description = description,
        descriptionTextColor = descriptionTextColor,
        enabled = enabled,
        minHeight = if (description != null) {
            MENU_ITEM_HEIGHT_WITH_DESC
        } else {
            MENU_ITEM_HEIGHT_WITHOUT_DESC
        },
        onClick = onClick,
        beforeIconPainter = beforeIconPainter,
        beforeIconDescription = beforeIconDescription,
        beforeIconTint = iconTint,
        showDivider = showDivider,
        afterIconPainter = afterIconPainter,
        afterIconDescription = afterIconDescription,
        afterIconTint = iconTint,
        onAfterIconClick = onAfterIconClick,
    )
}

/**
 * An [IconListItem] wrapper for menu items in a [MenuGroup] with an optional icon at the end.
 *
 * @param label The label in the menu item.
 * @param description An optional description text below the label.
 * @param onClick Invoked when the user clicks on the item.
 */
@Composable
internal fun MenuTextItem(
    label: String,
    description: String? = null,
    onClick: (() -> Unit)? = null,
) {
    TextListItem(
        label = label,
        maxLabelLines = 2,
        description = description,
        minHeight = if (description != null) {
            MENU_ITEM_HEIGHT_WITH_DESC
        } else {
            MENU_ITEM_HEIGHT_WITHOUT_DESC
        },
        onClick = onClick,
    )
}

/**
 * Enum containing all the supported state for the menu item.
 */
enum class MenuItemState {
    /**
     * The menu item is enabled.
     */
    ENABLED,

    /**
     * The menu item is disabled and is not clickable.
     */
    DISABLED,

    /**
     * The menu item is highlighted to indicate the feature behind the menu item is active.
     */
    ACTIVE,

    /**
     * The menu item is highlighted to indicate the feature behind the menu item is destructive.
     */
    WARNING,
}

@Composable
private fun getLabelTextColor(state: MenuItemState): Color {
    return when (state) {
        MenuItemState.ACTIVE -> FirefoxTheme.colors.textAccent
        MenuItemState.WARNING -> FirefoxTheme.colors.textCritical
        else -> FirefoxTheme.colors.textPrimary
    }
}

@Composable
private fun getDescriptionTextColor(state: MenuItemState): Color {
    return when (state) {
        MenuItemState.ACTIVE -> FirefoxTheme.colors.textAccent
        MenuItemState.WARNING -> FirefoxTheme.colors.textCritical
        MenuItemState.DISABLED -> FirefoxTheme.colors.textDisabled
        else -> FirefoxTheme.colors.textSecondary
    }
}

@Composable
private fun getIconTint(state: MenuItemState): Color {
    return when (state) {
        MenuItemState.ACTIVE -> FirefoxTheme.colors.iconAccentViolet
        MenuItemState.WARNING -> FirefoxTheme.colors.iconCritical
        else -> FirefoxTheme.colors.iconSecondary
    }
}

@LightDarkPreview
@Composable
private fun MenuItemPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3)
                .padding(16.dp),
        ) {
            MenuGroup {
                for (state in MenuItemState.entries) {
                    MenuItem(
                        label = stringResource(id = R.string.browser_menu_translations),
                        beforeIconPainter = painterResource(id = R.drawable.mozac_ic_translate_24),
                        state = state,
                        onClick = {},
                    )

                    Divider(color = FirefoxTheme.colors.borderSecondary)
                }

                for (state in MenuItemState.entries) {
                    MenuItem(
                        label = stringResource(id = R.string.browser_menu_extensions),
                        beforeIconPainter = painterResource(id = R.drawable.mozac_ic_extension_24),
                        state = state,
                        onClick = {},
                        afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
                    )

                    Divider(color = FirefoxTheme.colors.borderSecondary)
                }

                for (state in MenuItemState.entries) {
                    MenuItem(
                        label = stringResource(id = R.string.browser_menu_extensions),
                        beforeIconPainter = painterResource(id = R.drawable.mozac_ic_extension_24),
                        state = state,
                        onClick = {},
                        showDivider = true,
                        afterIconPainter = painterResource(id = R.drawable.mozac_ic_plus_24),
                        onAfterIconClick = {},
                    )

                    Divider(color = FirefoxTheme.colors.borderSecondary)
                }
            }
        }
    }
}
