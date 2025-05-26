/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(28.dp)

/**
 * A menu group container.
 *
 * @param content The children composable to be laid out.
 */
@Composable
internal fun MenuGroup(content: @Composable () -> Unit) {
    Column(
        modifier = Modifier
            .clip(shape = ROUNDED_CORNER_SHAPE),
        verticalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        content()
    }
}

@PreviewLightDark
@Composable
private fun MenuGroupPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .padding(16.dp),
        ) {
            MenuGroup {
                MenuItem(
                    label = stringResource(id = R.string.browser_menu_add_to_homescreen),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_plus_24),
                )

                MenuItem(
                    label = stringResource(id = R.string.browser_menu_add_to_homescreen),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_plus_24),
                )
            }
        }
    }
}
