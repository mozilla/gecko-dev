/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar

import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.theme.AcornTheme

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(8.dp)

/**
 * Sub-component of the [BrowserToolbar] responsible for displaying toolbar in the
 * middle of the homepage.
 *
 * @param url The URL to be displayed.
 * @param colors The color scheme to use in the browser display toolbar.
 * @param textStyle [TextStyle] configuration for the URL text.
 * @param onUrlClicked Will be called when the user clicks on the URL.
 */
@Composable
fun HomepageDisplayToolbar(
    url: String,
    textStyle: TextStyle = LocalTextStyle.current,
    onUrlClicked: () -> Unit = {},
) {
    Row(
        modifier = Modifier
            .background(color = AcornTheme.colors.layer1)
            .fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Spacer(modifier = Modifier.width(8.dp))

        Row(
            modifier = Modifier
                .padding(vertical = 8.dp)
                .border(
                    width = 1.dp,
                    color = AcornTheme.colors.borderPrimary,
                    shape = ROUNDED_CORNER_SHAPE,
                )
                .background(
                    color = AcornTheme.colors.layer3,
                    shape = ROUNDED_CORNER_SHAPE,
                )
                .weight(1f),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                url,
                color = AcornTheme.colors.textPrimary,
                modifier = Modifier
                    .clickable { onUrlClicked() }
                    .padding(8.dp)
                    .weight(1f),
                maxLines = 1,
                style = textStyle,
            )
        }

        Spacer(modifier = Modifier.width(8.dp))
    }
}

@PreviewLightDark
@Composable
private fun HomepageDisplayToolbarPreview() {
    AcornTheme {
        HomepageDisplayToolbar(
            url = "http://www.mozilla.org",
        )
    }
}
