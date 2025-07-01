/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.iconpicker.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.res.colorResource
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.compose.button.RadioButton
import org.mozilla.fenix.iconpicker.ActivityAlias
import org.mozilla.fenix.iconpicker.IconBackground
import org.mozilla.fenix.iconpicker.SettingsAppIcon
import org.mozilla.fenix.iconpicker.SettingsGroupTitle
import org.mozilla.fenix.theme.FirefoxTheme

private val ListItemHeight = 56.dp
private val AppIconSize = 40.dp
private val AppIconBorderWidth = 1.dp
private val GroupHeaderHeight = 36.dp
private val GroupHeaderPaddingStart = 72.dp
private val GroupSpacerHeight = 8.dp

/**
 * A composable that displays a list of app icon options.
 *
 * @param currentAppIcon The currently selected app icon alias.
 * @param groupedIconOptions Icons are displayed in sections under their respective titles.
 * @param onClick A callback invoked when an icon option is selected.
 */
@Composable
fun AppIconSelection(
    currentAppIcon: ActivityAlias,
    groupedIconOptions: Map<SettingsGroupTitle, List<SettingsAppIcon>>,
    onClick: (SettingsAppIcon) -> Unit,
) {
    Column(
        modifier = Modifier.background(color = FirefoxTheme.colors.layer1),
    ) {
        groupedIconOptions.forEach { (header, icons) ->
            AppIconGroupHeader(header)

            icons.forEach { icon ->
                AppIconOption(
                    appIcon = icon,
                    selected = icon.activityAlias == currentAppIcon,
                    onClick = onClick,
                )
            }

            Spacer(modifier = Modifier.height(GroupSpacerHeight))

            Divider(color = FirefoxTheme.colors.borderPrimary)
        }
    }
}

@Composable
private fun AppIconGroupHeader(title: SettingsGroupTitle) {
    Text(
        text = stringResource(id = title.titleId),
        modifier = Modifier
            .height(GroupHeaderHeight)
            .padding(start = GroupHeaderPaddingStart)
            .wrapContentHeight(Alignment.CenterVertically)
            .semantics { heading() },
        style = FirefoxTheme.typography.headline8,
        color = FirefoxTheme.colors.textAccent,
    )
}

@Composable
private fun AppIconOption(
    appIcon: SettingsAppIcon,
    selected: Boolean,
    onClick: (SettingsAppIcon) -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(ListItemHeight)
            .clickable { onClick(appIcon) },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        RadioButton(
            selected = selected,
            onClick = {
                // No-op, the whole item is clickable
            },
        )

        AppIcon(appIcon)

        Spacer(modifier = Modifier.width(16.dp))

        Text(
            text = stringResource(appIcon.titleId),
            modifier = Modifier.weight(1f),
            style = FirefoxTheme.typography.subtitle1,
            color = FirefoxTheme.colors.textPrimary,
        )
    }
}

@Composable
private fun AppIcon(appIcon: SettingsAppIcon) {
    val shape = RoundedCornerShape(4.dp)

    Box(
        modifier = Modifier
            .size(AppIconSize)
            .clip(shape)
            .border(AppIconBorderWidth, FirefoxTheme.colors.borderPrimary, shape),
    ) {
        when (val background = appIcon.activityAlias.iconBackground) {
            is IconBackground.Color -> {
                Box(
                    modifier = Modifier
                        .size(AppIconSize)
                        .background(colorResource(id = background.colorResId)),
                )
            }
            is IconBackground.Drawable -> {
                Image(
                    painter = painterResource(id = background.drawableResId),
                    contentDescription = null,
                    modifier = Modifier.size(AppIconSize),
                )
            }
        }

        Image(
            painter = painterResource(id = appIcon.activityAlias.iconForegroundId),
            contentDescription = null,
            modifier = Modifier.size(AppIconSize),
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun AppIconSelectionPreview() {
    FirefoxTheme {
        AppIconSelection(
            currentAppIcon = ActivityAlias.AppDefault,
            groupedIconOptions = SettingsAppIcon.groupedAppIcons,
            onClick = {},
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun AppIconOptionPreview() {
    val sampleItem = SettingsAppIcon.groupedAppIcons
        .values
        .flatten()
        .firstOrNull()!!

    FirefoxTheme {
        AppIconOption(sampleItem, false) {}
    }
}
