/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.material.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import mozilla.components.compose.base.button.IconButton
import mozilla.components.compose.base.button.LongPressIconButton
import mozilla.components.compose.base.menu.CustomPlacementPopup
import mozilla.components.compose.base.menu.CustomPlacementPopupVerticalContent
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.CombinedEventAndMenu
import mozilla.components.compose.browser.toolbar.ui.MenuState.CLick
import mozilla.components.compose.browser.toolbar.ui.MenuState.LongClick
import mozilla.components.compose.browser.toolbar.ui.MenuState.None
import mozilla.components.ui.icons.R

/**
 * A clickable icon used to represent an action that can be added to the toolbar.
 *
 * @param icon Drawable resource for this button.
 * @param contentDescription Text used by accessibility services to describe what this button does.
 * @param isActive Whether or not to show this button as a currently active feature.
 * @param highlighted Whether or not to highlight this button.
 * @param onClick [BrowserToolbarInteraction] describing how to handle this button being clicked.
 * @param onLongClick Optional [BrowserToolbarInteraction] describing how to handle this button being long clicked.
 * @param onInteraction Callback for handling [BrowserToolbarEvent]s on user interactions.
 */
@Composable
@Suppress("LongMethod")
fun ActionButton(
    @DrawableRes icon: Int,
    @StringRes contentDescription: Int,
    isActive: Boolean = false,
    highlighted: Boolean = false,
    onClick: BrowserToolbarInteraction,
    onLongClick: BrowserToolbarInteraction? = null,
    onInteraction: (BrowserToolbarEvent) -> Unit,
) {
    val shouldReactToLongClicks = remember(onLongClick) {
        onLongClick != null
    }
    var currentMenuState by remember { mutableStateOf(None) }
    val colors = AcornTheme.colors
    val tint = remember(isActive, colors) {
        when (isActive) {
            true -> colors.iconAccentViolet
            false -> colors.iconPrimary
        }
    }

    val handleInteraction: (BrowserToolbarInteraction) -> Unit = { interaction ->
        when (interaction) {
            is BrowserToolbarEvent -> onInteraction(interaction)
            is BrowserToolbarMenu -> {
                when (interaction) {
                    onClick -> currentMenuState = CLick
                    onLongClick -> currentMenuState = LongClick
                    else -> {
                        // no-op. Not possible, just making the compiler happy.
                    }
                }
            }
            is CombinedEventAndMenu -> {
                onInteraction(interaction.event)
                currentMenuState = LongClick
            }
        }
    }

    when (shouldReactToLongClicks) {
        true -> LongPressIconButton(
            onClick = { handleInteraction(onClick) },
            onLongClick = {
                if (onLongClick != null) {
                    handleInteraction(onLongClick)
                }
            },
            contentDescription = stringResource(contentDescription),
        ) {
            Box {
                ActionButtonIcon(icon, tint)
                if (highlighted) {
                    DotHighlight(
                        modifier = Modifier.align(Alignment.BottomEnd),
                    )
                }
            }

            ActionButtonMenu(
                currentMenuState = currentMenuState,
                wantedMenu = LongClick,
                popupData = onLongClick,
                onInteraction = onInteraction,
                onDismissRequest = { currentMenuState = None },
            )
        }

        false -> IconButton(
            onClick = { handleInteraction(onClick) },
            contentDescription = stringResource(contentDescription),
        ) {
            Box {
                ActionButtonIcon(icon, tint)
                if (highlighted) {
                    DotHighlight(
                        modifier = Modifier.align(Alignment.BottomEnd),
                    )
                }
            }

            ActionButtonMenu(
                currentMenuState = currentMenuState,
                wantedMenu = CLick,
                popupData = onClick,
                onInteraction = onInteraction,
                onDismissRequest = { currentMenuState = None },
            )
        }
    }
}

@Composable
private fun ActionButtonIcon(
    @DrawableRes icon: Int,
    tint: Color,
) {
    Icon(
        painter = painterResource(icon),
        contentDescription = null,
        tint = tint,
    )
}

@Composable
private inline fun ActionButtonMenu(
    currentMenuState: MenuState,
    wantedMenu: MenuState,
    popupData: BrowserToolbarInteraction?,
    crossinline onInteraction: (BrowserToolbarEvent) -> Unit,
    noinline onDismissRequest: () -> Unit,
) {
    if (currentMenuState == wantedMenu) {
        CustomPlacementPopup(
            isVisible = true,
            onDismissRequest = onDismissRequest,
        ) {
            popupData?.let {
                CustomPlacementPopupVerticalContent {
                    it.toMenuItems().forEach { menuItem ->
                        menuItemComposable(menuItem) { event ->
                            onDismissRequest()
                            onInteraction(event)
                        }.invoke()
                    }
                }
            }
        }
    }
}

private enum class MenuState {
    None, CLick, LongClick
}

@PreviewLightDark
@Composable
private fun ActionButtonPreview() {
    AcornTheme {
        Box(modifier = Modifier.background(AcornTheme.colors.layer1)) {
            ActionButton(
                icon = R.drawable.mozac_ic_web_extension_default_icon,
                contentDescription = R.string.mozac_error_confused,
                onClick = object : BrowserToolbarEvent {},
                onInteraction = {},
            )
        }
    }
}

@PreviewLightDark
@Composable
private fun HighlightedActionButtonPreview() {
    AcornTheme {
        Box(modifier = Modifier.background(AcornTheme.colors.layer1)) {
            ActionButton(
                icon = R.drawable.mozac_ic_web_extension_default_icon,
                contentDescription = R.string.mozac_error_confused,
                onClick = object : BrowserToolbarEvent {},
                highlighted = true,
                onInteraction = {},
            )
        }
    }
}
