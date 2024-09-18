/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.padding
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import mozilla.components.feature.addons.Addon
import mozilla.components.feature.addons.ui.displayName
import mozilla.components.feature.addons.ui.summary
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.compose.list.FaviconListItem
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.translations.rotationAnimation

/**
 * An [Addon] menu item.
 *
 * @param addon The [Addon] to display.
 * @param addonInstallationInProgress Whether or not [Addon] installation is in progress.
 * @param onClick Invoked when the user clicks on the item.
 * @param onIconClick Invoked when the user clicks on the icon button.
 */
@Composable
internal fun AddonMenuItem(
    addon: Addon,
    addonInstallationInProgress: Addon?,
    onClick: () -> Unit,
    onIconClick: () -> Unit,
) {
    val context = LocalContext.current
    val label = addon.displayName(context)
    val description = addon.summary(context)
    val addonIcon = addon.provideIcon()
    val isInstallAddonInProgress = addon == addonInstallationInProgress

    if (addonIcon != null) {
        FaviconListItem(
            label = label,
            url = addon.iconUrl,
            description = description,
            faviconPainter = BitmapPainter(image = addonIcon.asImageBitmap()),
            onClick = onClick,
            showDivider = true,
            iconPainter = if (isInstallAddonInProgress) {
                painterResource(id = R.drawable.mozac_ic_sync_24)
            } else {
                painterResource(id = R.drawable.mozac_ic_plus_24)
            },
            iconButtonModifier = if (isInstallAddonInProgress) {
                Modifier.rotate(rotationAnimation())
            } else {
                Modifier
            },
            iconDescription = stringResource(R.string.browser_menu_extension_plus_icon_content_description),
            onIconClick = onIconClick,
        )
    } else {
        MenuItem(
            label = label,
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_extension_24),
            description = description,
            onClick = onClick,
            showDivider = true,
            afterIconPainter = painterResource(id = R.drawable.mozac_ic_plus_24),
            afterIconDescription = stringResource(R.string.browser_menu_extension_plus_icon_content_description),
            onAfterIconClick = onIconClick,
        )
    }
}

@LightDarkPreview
@Composable
private fun AddonMenuItemPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3)
                .padding(16.dp),
        ) {
            MenuGroup {
                AddonMenuItem(
                    addon = Addon(
                        id = "id",
                        translatableName = mapOf(Addon.DEFAULT_LOCALE to "name"),
                        translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description"),
                        translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary"),
                    ),
                    addonInstallationInProgress = Addon(
                        id = "id",
                        translatableName = mapOf(Addon.DEFAULT_LOCALE to "name"),
                        translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description"),
                        translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary"),
                    ),
                    onClick = {},
                    onIconClick = {},
                )
            }
        }
    }
}
