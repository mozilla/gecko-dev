/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import android.graphics.drawable.Drawable
import androidx.appcompat.content.res.AppCompatResources
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.PreviewLightDark
import com.google.accompanist.drawablepainter.rememberDrawablePainter
import mozilla.components.compose.base.button.IconButton
import mozilla.components.compose.base.button.LongPressIconButton
import mozilla.components.compose.base.menu.CustomPlacementPopup
import mozilla.components.compose.base.menu.CustomPlacementPopupVerticalContent
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton.State
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
 * @param icon A [Drawable] to use as icon for this button.
 * @param contentDescription A [String] to use as content description for this button.
 * @param state The current [State] of the action button.
 * @param highlighted Whether or not to highlight this button.
 * @param onClick [BrowserToolbarInteraction] describing how to handle this button being clicked.
 * @param onLongClick Optional [BrowserToolbarInteraction] describing how to handle this button being long clicked.
 * @param onInteraction Callback for handling [BrowserToolbarEvent]s on user interactions.
 */
@Composable
@Suppress("LongMethod", "CyclomaticComplexMethod")
internal fun ActionButton(
    icon: Drawable,
    contentDescription: String,
    state: State = State.DEFAULT,
    shouldTint: Boolean = true,
    highlighted: Boolean = false,
    onClick: BrowserToolbarInteraction? = null,
    onLongClick: BrowserToolbarInteraction? = null,
    onInteraction: (BrowserToolbarEvent) -> Unit,
) {
    val shouldReactToLongClicks = remember(onLongClick) {
        onLongClick != null
    }
    var currentMenuState by remember { mutableStateOf(None) }
    val colors = AcornTheme.colors
    val tint = remember(state, colors) {
        when (state) {
            State.ACTIVE -> colors.iconAccentViolet
            State.DISABLED -> colors.iconDisabled
            State.DEFAULT -> colors.iconPrimary
        }
    }

    val isEnabled = remember(state) {
        when (state) {
            State.DISABLED -> false
            else -> true
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
            onClick = {
                if (onClick != null) {
                    handleInteraction(onClick)
                }
            },
            onLongClick = {
                if (onLongClick != null) {
                    handleInteraction(onLongClick)
                }
            },
            enabled = isEnabled,
            contentDescription = contentDescription,
        ) {
            Box {
                ActionButtonIcon(icon, tint, shouldTint)
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
            onClick = {
                if (onClick != null) {
                    handleInteraction(onClick)
                }
            },
            enabled = isEnabled,
            contentDescription = contentDescription,
        ) {
            Box {
                ActionButtonIcon(icon, tint, shouldTint)
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
    icon: Drawable,
    tint: Color,
    shouldTint: Boolean,
) {
    Image(
        painter = rememberDrawablePainter(icon),
        contentDescription = null,
        contentScale = ContentScale.Crop,
        colorFilter = when (shouldTint) {
            true -> ColorFilter.tint(tint)
            else -> null
        },
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
                icon = AppCompatResources.getDrawable(
                    LocalContext.current,
                    R.drawable.mozac_ic_web_extension_default_icon,
                )!!,
                contentDescription = "Test",
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
                icon = AppCompatResources.getDrawable(
                    LocalContext.current,
                    R.drawable.mozac_ic_web_extension_default_icon,
                )!!,
                contentDescription = "Test",
                onClick = object : BrowserToolbarEvent {},
                highlighted = true,
                onInteraction = {},
            )
        }
    }
}
