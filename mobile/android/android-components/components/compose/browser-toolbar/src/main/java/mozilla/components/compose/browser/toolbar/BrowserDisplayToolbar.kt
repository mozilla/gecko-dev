/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.LocalTextStyle
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.concept.Action
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(8.dp)

/**
 * Sub-component of the [BrowserToolbar] responsible for displaying the URL and related
 * controls ("display mode").
 *
 * @param url The URL to be displayed.
 * @param colors The color scheme to use in the browser display toolbar.
 * @param textStyle [TextStyle] configuration for the URL text.
 * @param navigationActions List of navigation [Action]s to be displayed on left side of the
 * display toolbar (outside of the URL bounding box).
 * @param pageActions List of page [Action]s to be displayed to the right side of the URL of the
 * display toolbar. Also see:
 * [MDN docs](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction)
 * @param browserActions List of browser [Action]s to be displayed on the right side of the
 * display toolbar (outside of the URL bounding box). Also see:
 * [MDN docs](https://developer.mozilla.org/en-US/Add-ons/WebExtensions/user_interface/Browser_action)
 * @param onUrlClicked Will be called when the user clicks on the URL.
 * @param onInteraction Callback for handling [BrowserToolbarEvent]s on user interactions.
 */
@Composable
fun BrowserDisplayToolbar(
    url: String,
    colors: BrowserDisplayToolbarColors,
    textStyle: TextStyle = LocalTextStyle.current,
    navigationActions: List<Action> = emptyList(),
    pageActions: List<Action> = emptyList(),
    browserActions: List<Action> = emptyList(),
    onUrlClicked: () -> Unit = {},
    onInteraction: (BrowserToolbarEvent) -> Unit = {},
) {
    Row(
        modifier = Modifier
            .background(color = colors.background)
            .fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (navigationActions.isNotEmpty()) {
            ActionContainer(
                actions = navigationActions,
                onInteraction = onInteraction,
            )
        } else {
            Spacer(modifier = Modifier.width(8.dp))
        }

        Row(
            modifier = Modifier
                .padding(vertical = 8.dp)
                .background(
                    color = colors.urlBackground,
                    shape = ROUNDED_CORNER_SHAPE,
                )
                .weight(1f),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                url,
                color = colors.text,
                modifier = Modifier
                    .clickable { onUrlClicked() }
                    .padding(8.dp)
                    .weight(1f),
                maxLines = 1,
                style = textStyle,
            )

            ActionContainer(
                actions = pageActions,
                onInteraction = onInteraction,
            )
        }

        if (browserActions.isNotEmpty()) {
            ActionContainer(
                actions = browserActions,
                onInteraction = onInteraction,
            )
        } else {
            Spacer(modifier = Modifier.width(8.dp))
        }
    }
}

@PreviewLightDark
@Composable
private fun BrowserDisplayToolbarPreview() {
    AcornTheme {
        BrowserDisplayToolbar(
            url = "http://www.mozilla.org",
            colors = BrowserDisplayToolbarColors(
                background = AcornTheme.colors.layer1,
                urlBackground = AcornTheme.colors.layer3,
                text = AcornTheme.colors.textPrimary,
            ),
            navigationActions = listOf(
                Action.ActionButton(
                    icon = mozilla.components.ui.icons.R.drawable.mozac_ic_home_24,
                    contentDescription = null,
                    tint = AcornTheme.colors.iconPrimary.toArgb(),
                    onClick = {},
                ),
            ),
            pageActions = listOf(
                Action.ActionButton(
                    icon = mozilla.components.ui.icons.R.drawable.mozac_ic_arrow_clockwise_24,
                    contentDescription = null,
                    tint = AcornTheme.colors.iconPrimary.toArgb(),
                    onClick = {},
                ),
            ),
            browserActions = listOf(
                Action.ActionButton(
                    icon = mozilla.components.ui.icons.R.drawable.mozac_ic_ellipsis_vertical_24,
                    contentDescription = null,
                    tint = AcornTheme.colors.iconPrimary.toArgb(),
                    onClick = {},
                ),
            ),
        )
    }
}
