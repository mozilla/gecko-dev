/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.requiredSize
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.concept.Action.ActionButton
import mozilla.components.compose.browser.toolbar.concept.Action.CustomAction
import mozilla.components.compose.browser.toolbar.concept.Action.DropdownAction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.ui.SearchSelector
import mozilla.components.ui.icons.R

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
                    ActionButton(action)
                }
                is CustomAction -> {
                    action.content()
                }

                is DropdownAction -> {
                    SearchSelector(
                        icon = action.icon,
                        contentDescription = stringResource(action.contentDescription),
                        menu = action.menu,
                        onInteraction = { onInteraction(it) },
                    )
                }
            }
        }
    }
}

@Composable
private fun ActionButton(
    action: ActionButton,
) {
    IconButton(
        modifier = Modifier.requiredSize(40.dp),
        onClick = { action.onClick() },
    ) {
        Icon(
            painter = painterResource(action.icon),
            contentDescription = action.contentDescription,
            tint = Color(action.tint),
        )
    }
}

@PreviewLightDark
@Composable
private fun ActionContainerPreview() {
    AcornTheme {
        ActionContainer(
            actions = listOf(
                ActionButton(
                    icon = R.drawable.mozac_ic_microphone_24,
                    contentDescription = null,
                    tint = AcornTheme.colors.iconPrimary.toArgb(),
                    onClick = {},
                ),
                CustomAction(
                    content = {
                        SearchSelector(
                            painter = painterResource(R.drawable.mozac_ic_search_24),
                            tint = AcornTheme.colors.iconPrimary,
                            onClick = {},
                        )
                    },
                ),
            ),
            onInteraction = {},
        )
    }
}
