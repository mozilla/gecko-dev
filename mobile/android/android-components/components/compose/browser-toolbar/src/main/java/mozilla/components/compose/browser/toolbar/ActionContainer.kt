/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import android.graphics.drawable.Drawable
import androidx.appcompat.content.res.AppCompatResources
import androidx.compose.foundation.layout.Row
import androidx.compose.runtime.Composable
import androidx.compose.runtime.ReadOnlyComposable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButtonRes
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.ContentDescription.StringContentDescription
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.ContentDescription.StringResContentDescription
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.Icon.DrawableIcon
import mozilla.components.compose.browser.toolbar.concept.Action.SearchSelectorAction.Icon.DrawableResIcon
import mozilla.components.compose.browser.toolbar.concept.Action.TabCounterAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.ui.SearchSelector
import mozilla.components.compose.browser.toolbar.ui.TabCounter
import mozilla.components.compose.browser.toolbar.ui.ActionButton as ActionButtonComposable
import mozilla.components.ui.icons.R as iconsR

/**
 * A container for displaying [Action]s.
 *
 * @param actions List of [Action]s to display in the container.
 * @param onInteraction Callback for handling [BrowserToolbarEvent]s on user interactions.
 */
@Composable
fun ActionContainer(
    actions: List<Action>,
    onInteraction: (BrowserToolbarEvent) -> Unit,
) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        for (action in actions) {
            when (action) {
                is ActionButtonRes -> {
                    action.iconDrawable()?.let {
                        ActionButtonComposable(
                            icon = it,
                            contentDescription = stringResource(action.contentDescription),
                            state = action.state,
                            onClick = action.onClick,
                            highlighted = action.highlighted,
                            onLongClick = action.onLongClick,
                            onInteraction = { onInteraction(it) },
                        )
                    }
                }

                is ActionButton -> {
                    action.iconDrawable()?.let {
                        ActionButtonComposable(
                            icon = it,
                            contentDescription = action.contentDescription,
                            state = action.state,
                            onClick = action.onClick,
                            highlighted = action.highlighted,
                            onLongClick = action.onLongClick,
                            onInteraction = { onInteraction(it) },
                        )
                    }
                }

                is SearchSelectorAction -> {
                    SearchSelector(
                        icon = action.iconDrawable(),
                        shouldTint = (action.icon as? DrawableIcon)?.shouldTint ?: true,
                        contentDescription = action.contentDescription(),
                        menu = action.menu,
                        onInteraction = { onInteraction(it) },
                        onClick = action.onClick,
                    )
                }

                is TabCounterAction -> {
                    TabCounter(
                        count = action.count,
                        showPrivacyMask = action.showPrivacyMask,
                        onClick = action.onClick,
                        onLongClick = action.onLongClick,
                        onInteraction = { onInteraction(it) },
                    )
                }
            }
        }
    }
}

@Composable
private fun ActionButtonRes.iconDrawable(): Drawable? {
    val context = LocalContext.current
    val tint = AcornTheme.colors.iconPrimary

    return remember(this, context) {
        AppCompatResources.getDrawable(context, drawableResId)
            ?.apply { mutate().setTint(tint.toArgb()) }
    }
}

@Composable
private fun ActionButton.iconDrawable(): Drawable? {
    val tint = AcornTheme.colors.iconPrimary

    return remember(this) {
        when (shouldTint) {
            true -> drawable?.mutate()?.apply { setTint(tint.toArgb()) }
            false -> drawable
        }
    }
}

@Composable
@ReadOnlyComposable
private fun SearchSelectorAction.contentDescription() = when (contentDescription) {
    is StringContentDescription -> contentDescription.text
    is StringResContentDescription -> stringResource(contentDescription.resourceId)
}

@Composable
private fun SearchSelectorAction.iconDrawable(): Drawable? {
    val context = LocalContext.current
    val tint = AcornTheme.colors.iconPrimary

    val drawable = remember(this, context) {
        when (icon) {
            is DrawableIcon -> icon.drawable
            is DrawableResIcon -> AppCompatResources.getDrawable(context, icon.resourceId)
                ?.apply { setTint(tint.toArgb()) }
        }
    }
    return drawable
}

@Preview(showBackground = true)
@Composable
private fun ActionContainerPreview() {
    AcornTheme {
        ActionContainer(
            actions = listOf(
                SearchSelectorAction(
                    icon = DrawableResIcon(iconsR.drawable.mozac_ic_search_24),
                    contentDescription = StringContentDescription("Change search engine for this search"),
                    menu = { emptyList() },
                    onClick = null,
                ),
                ActionButtonRes(
                    drawableResId = iconsR.drawable.mozac_ic_microphone_24,
                    contentDescription = R.string.mozac_clear_button_description,
                    onClick = object : BrowserToolbarEvent {},
                ),
                ActionButton(
                    drawable = AppCompatResources.getDrawable(LocalContext.current, iconsR.drawable.mozac_ic_tool_24),
                    contentDescription = stringResource(R.string.mozac_clear_button_description),
                    onClick = object : BrowserToolbarEvent {},
                ),
                TabCounterAction(
                    count = 1,
                    contentDescription = "",
                    showPrivacyMask = false,
                    onClick = object : BrowserToolbarEvent {},
                ),
            ),
            onInteraction = {},
        )
    }
}
