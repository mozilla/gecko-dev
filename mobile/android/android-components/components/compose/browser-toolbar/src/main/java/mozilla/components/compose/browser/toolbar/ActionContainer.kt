/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.foundation.layout.Row
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.core.content.ContextCompat
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.concept.Action.DropdownAction
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
                is ActionButton -> {
                    ActionButtonComposable(
                        icon = action.icon,
                        contentDescription = action.contentDescription,
                        tint = action.tint,
                        onClick = action.onClick,
                        onLongClick = action.onLongClick,
                        onInteraction = { onInteraction(it) },
                    )
                }

                is DropdownAction -> {
                    SearchSelector(
                        icon = action.icon,
                        iconResource = action.iconResource,
                        contentDescription = stringResource(action.contentDescription),
                        menu = action.menu,
                        onInteraction = { onInteraction(it) },
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

@Preview(showBackground = true)
@Composable
private fun ActionContainerPreview() {
    AcornTheme {
        ActionContainer(
            actions = listOf(
                DropdownAction(
                    icon = ContextCompat.getDrawable(LocalContext.current, iconsR.drawable.mozac_ic_search_24)!!,
                    contentDescription = R.string.mozac_clear_button_description,
                    menu = { emptyList() },
                ),
                ActionButton(
                    icon = iconsR.drawable.mozac_ic_microphone_24,
                    contentDescription = R.string.mozac_clear_button_description,
                    tint = AcornTheme.colors.iconPrimary.toArgb(),
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
