/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.Surface
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.Hyphens
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * A [Surface]-backed menu item used in the library menu group, displaying an icon above a label
 * and automatically adapting its background, icon tint, and text color according to [MenuItemState].
 *
 * @param modifier Modifier to apply to the root Surface container.
 * @param iconRes Drawable resource ID for the icon.
 * @param labelRes String resource ID for the label text.
 * @param state The state of the menu item to display.
 * @param shape The [RoundedCornerShape] to clip the background into.
 * @param onClick Invoked when the user taps this item.
 */
@Composable
fun LibraryMenuItem(
    modifier: Modifier = Modifier,
    @DrawableRes iconRes: Int,
    @StringRes labelRes: Int,
    state: MenuItemState = MenuItemState.ENABLED,
    shape: RoundedCornerShape = RoundedCornerShape(4.dp),
    onClick: () -> Unit,
) {
    val labelColor = when (state) {
        MenuItemState.ACTIVE -> FirefoxTheme.colors.textAccent
        MenuItemState.WARNING -> FirefoxTheme.colors.textCritical
        else -> FirefoxTheme.colors.textPrimary
    }
    val iconTint = when (state) {
        MenuItemState.ACTIVE -> FirefoxTheme.colors.iconAccentViolet
        MenuItemState.WARNING -> FirefoxTheme.colors.iconCritical
        else -> FirefoxTheme.colors.iconSecondary
    }

    Surface(
        modifier = modifier
            .fillMaxWidth()
            .clickable(enabled = state != MenuItemState.DISABLED, onClick = onClick),
        color = FirefoxTheme.colors.layer3,
        contentColor = labelColor,
        shape = shape,
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = Modifier.padding(12.dp),
        ) {
            Icon(
                painter = painterResource(iconRes),
                contentDescription = stringResource(labelRes),
                tint = iconTint,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                text = stringResource(labelRes),
                style = FirefoxTheme.typography.caption.copy(
                    hyphens = Hyphens.Auto,
                ),
                modifier = Modifier
                    .fillMaxWidth(),
                textAlign = TextAlign.Center,
                maxLines = 2,
                softWrap = true,
            )
        }
    }
}
