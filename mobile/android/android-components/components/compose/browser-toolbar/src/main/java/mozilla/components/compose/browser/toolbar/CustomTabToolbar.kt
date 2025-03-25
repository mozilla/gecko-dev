/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.ui.icons.R as iconsR

/**
 * Sub-component of the [BrowserToolbar] responsible for displaying the custom tab.
 *
 * @param url The URL to be displayed.
 * @param title The title to be displayed.
 * @param colors The color scheme to use in the custom tab toolbar.
 * @param navigationActions List of navigation [Action]s to be displayed on left side of the
 * display toolbar (outside of the URL bounding box).
 * @param pageActions List of page [Action]s to be displayed to the right side of the URL of the
 * display toolbar. Also see:
 * [MDN docs](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction)
 * @param browserActions List of browser [Action]s to be displayed on the right side of the
 * display toolbar (outside of the URL bounding box). Also see:
 * [MDN docs](https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Browser_action)
 */
@Composable
fun CustomTabToolbar(
    url: String,
    title: String,
    colors: CustomTabToolbarColors,
    navigationActions: List<Action> = emptyList(),
    pageActions: List<Action> = emptyList(),
    browserActions: List<Action> = emptyList(),
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(56.dp)
            .background(color = colors.background),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        ActionContainer(
            actions = navigationActions,
            onInteraction = {},
        )

        Column(
            modifier = Modifier.weight(1f),
            verticalArrangement = Arrangement.Center,
        ) {
            if (title.isNotEmpty()) {
                Text(
                    text = title,
                    color = colors.title,
                    maxLines = 1,
                    style = AcornTheme.typography.headline8,
                )
            }

            if (url.isNotEmpty()) {
                Text(
                    text = url,
                    color = colors.url,
                    maxLines = 1,
                    style = AcornTheme.typography.caption,
                )
            }
        }

        ActionContainer(
            actions = pageActions,
            onInteraction = {},
        )

        ActionContainer(
            actions = browserActions,
            onInteraction = {},
        )
    }
}

@PreviewLightDark
@Composable
private fun CustomTabToolbarPreview() {
    AcornTheme {
        CustomTabToolbar(
            url = "http://www.mozilla.org",
            title = "Mozilla",
            colors = CustomTabToolbarColors(
                background = AcornTheme.colors.layer1,
                title = AcornTheme.colors.textPrimary,
                url = AcornTheme.colors.textSecondary,
            ),
            navigationActions = listOf(
                Action.ActionButton(
                    icon = iconsR.drawable.mozac_ic_cross_24,
                    contentDescription = null,
                    tint = AcornTheme.colors.iconPrimary.toArgb(),
                    onClick = {},
                ),
            ),
            browserActions = listOf(
                Action.ActionButton(
                    icon = iconsR.drawable.mozac_ic_arrow_clockwise_24,
                    contentDescription = null,
                    tint = AcornTheme.colors.iconPrimary.toArgb(),
                    onClick = {},
                ),
            ),
        )
    }
}
