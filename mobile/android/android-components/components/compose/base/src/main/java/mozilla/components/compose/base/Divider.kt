/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.LightDarkPreview
import mozilla.components.compose.base.theme.AcornTheme

/**
 * Generic divider.
 *
 * @param modifier [Modifier] used to be applied to the layout of the divider.
 * @param color [Color] of the divider line.
 */
@Composable
fun Divider(
    modifier: Modifier = Modifier,
    color: Color = AcornTheme.colors.borderPrimary,
) {
    androidx.compose.material.Divider(
        modifier = modifier,
        color = color,
    )
}

/**
 * An example of a vertical divider.
 */
@Composable
@LightDarkPreview
private fun VerticalDividerPreview() {
    AcornTheme {
        Box(
            Modifier
                .background(AcornTheme.colors.layer1)
                .height(75.dp),
        ) {
            Row {
                Text(
                    text = "Before the line",
                    modifier = Modifier.padding(end = 10.dp),
                )

                Divider(
                    modifier = Modifier
                        .fillMaxHeight()
                        .width(0.5.dp)
                        .padding(vertical = 10.dp),
                )

                Text(
                    text = "After the line",
                    modifier = Modifier.padding(start = 10.dp),
                )
            }
        }
    }
}

/**
 * An example of divider usage in a list menu.
 */
@Composable
@LightDarkPreview
private fun HorizontalDividerPreview() {
    AcornTheme {
        Box(
            Modifier
                .background(AcornTheme.colors.layer1)
                .width(100.dp)
                .height(175.dp),
        ) {
            Column(Modifier.padding(start = 4.dp)) {
                Text(text = "New")

                Text(text = "Open")

                Text(text = "Open Recent")

                Divider(modifier = Modifier.padding(vertical = 10.dp, horizontal = 24.dp))

                Text(text = "Close")

                Text(text = "Save")

                Text(text = "Save as")

                Text(text = "Rename")

                Divider(modifier = Modifier.padding(vertical = 10.dp, horizontal = 24.dp))
            }
        }
    }
}
