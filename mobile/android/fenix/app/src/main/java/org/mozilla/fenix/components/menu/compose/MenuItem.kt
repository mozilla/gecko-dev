/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.clearAndSetSemantics
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.role
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.Divider
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.list.IconListItem
import org.mozilla.fenix.compose.list.ImageListItem
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme

private val MENU_ITEM_HEIGHT_WITHOUT_DESC = 52.dp

private val MENU_ITEM_HEIGHT_WITH_DESC = 56.dp

private val BADGE_ROUNDED_CORNER = 100.dp

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(4.dp)

/**
 * An [IconListItem] wrapper for menu items in a [MenuGroup] with an optional icon at the end.
 *
 * @param label The label in the menu item.
 * @param beforeIconPainter [Painter] used to display an [Icon] before the list item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param labelModifier [Modifier] to be applied to the label.
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
 * @param afterContent Optional Composable for adding UI to the end of the list item.
 */
@Composable
internal fun MenuItem(
    label: String,
    beforeIconPainter: Painter,
    modifier: Modifier = Modifier,
    labelModifier: Modifier = Modifier,
    beforeIconDescription: String? = null,
    description: String? = null,
    state: MenuItemState = MenuItemState.ENABLED,
    descriptionState: MenuItemState = MenuItemState.ENABLED,
    onClick: (() -> Unit)? = null,
    showDivider: Boolean = false,
    afterIconPainter: Painter? = null,
    afterIconDescription: String? = null,
    onAfterIconClick: (() -> Unit)? = null,
    afterContent: (@Composable RowScope.() -> Unit)? = null,
) {
    val labelTextColor = getLabelTextColor(state = state)
    val descriptionTextColor = getDescriptionTextColor(state = descriptionState)
    val iconTint = getIconTint(state = state)
    val enabled = state != MenuItemState.DISABLED

    IconListItem(
        label = label,
        modifier = modifier
            .clickable(
                interactionSource = remember { MutableInteractionSource() },
                indication = LocalIndication.current,
                enabled = enabled,
            ) { onClick?.invoke() }
            .clearAndSetSemantics {
                role = Role.Button
                if (description != null) {
                    this.contentDescription = label + description
                } else {
                    this.contentDescription = label
                }
            }
            .wrapContentSize()
            .clip(shape = ROUNDED_CORNER_SHAPE)
            .background(
                color = FirefoxTheme.colors.layer3,
            ),
        labelModifier = labelModifier,
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
        afterListAction = afterContent,
    )
}

/**
 * An [IconListItem] wrapper for menu items in a [MenuGroup] with an optional icon at the end.
 *
 * @param label The label in the menu item.
 * @param modifier [Modifier] to be applied to the layout.
 * @param description An optional description text below the label.
 * @param iconPainter [Painter] used to display an [Icon] after the list item.
 * @param onClick Invoked when the user clicks on the item.
 */
@Composable
internal fun MenuTextItem(
    label: String,
    modifier: Modifier = Modifier,
    description: String? = null,
    iconPainter: Painter? = null,
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
        modifier = modifier
            .clip(shape = ROUNDED_CORNER_SHAPE)
            .background(
                color = FirefoxTheme.colors.layer3,
            ),
        iconPainter = iconPainter,
        onClick = onClick,
    )
}

/**
 * A Web extension menu item used to display an image with a title and a badge.
 *
 * @param label The label in the list item.
 * @param iconPainter [Painter] used to display an [Icon] before the list item.
 * @param enabled Controls the enabled state of the list item. When `false`, the list item will not
 * be clickable.
 * @param badgeText WebExtension badge text.
 * @param onClick Called when the user clicks on the item.
 * @param onSettingsClick Called when the user clicks on the settings icon.
 */
@Composable
internal fun WebExtensionMenuItem(
    label: String,
    iconPainter: Painter,
    enabled: Boolean?,
    badgeText: String?,
    onClick: (() -> Unit)? = null,
    onSettingsClick: (() -> Unit)? = null,
) {
    ImageListItem(
        label = label,
        iconPainter = iconPainter,
        enabled = enabled == true,
        onClick = onClick,
        modifier = Modifier.clickable(
            interactionSource = remember { MutableInteractionSource() },
            indication = LocalIndication.current,
            enabled = enabled == true,
        ) { onClick?.invoke() }
            .clearAndSetSemantics {
                role = Role.Button
                this.contentDescription = label
            }
            .wrapContentSize()
            .clip(shape = ROUNDED_CORNER_SHAPE)
            .background(
                color = FirefoxTheme.colors.layer3,
            ),
        afterListItemAction = {
            Row(
                modifier = Modifier.padding(start = 16.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(16.dp, Alignment.End),
            ) {
                if (!badgeText.isNullOrEmpty()) {
                    Badge(
                        badgeText = badgeText,
                        badgeBackgroundColor = FirefoxTheme.colors.layerSearch,
                    )
                }

                Divider(
                    modifier = Modifier
                        .padding(vertical = 6.dp)
                        .fillMaxHeight()
                        .width(1.dp),
                    color = FirefoxTheme.colors.borderPrimary,
                )

                IconButton(
                    modifier = Modifier.size(24.dp),
                    onClick = onSettingsClick ?: {},
                ) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_settings_24),
                        tint = FirefoxTheme.colors.iconPrimary,
                        contentDescription = null,
                    )
                }
            }
        },
    )
}

@Composable
internal fun Badge(
    badgeText: String,
    state: MenuItemState = MenuItemState.ENABLED,
    badgeBackgroundColor: Color?,
) {
    Column(
        modifier = Modifier
            .clip(shape = RoundedCornerShape(BADGE_ROUNDED_CORNER))
            .background(
                color = badgeBackgroundColor ?: FirefoxTheme.colors.layer2,
            )
            .padding(horizontal = 16.dp, vertical = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(
            text = badgeText,
            color = getLabelTextColor(state),
            overflow = TextOverflow.Ellipsis,
            style = FirefoxTheme.typography.subtitle2,
            maxLines = 1,
        )
    }
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

@PreviewLightDark
@Composable
private fun WebExtensionMenuItemPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer2),
        ) {
            WebExtensionMenuItem(
                label = "label",
                iconPainter = painterResource(R.drawable.mozac_ic_web_extension_default_icon),
                enabled = true,
                badgeText = "17",
                onClick = {},
                onSettingsClick = {},
            )
        }
    }
}

@PreviewLightDark
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
