/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.MenuGroup
import org.mozilla.fenix.components.menu.compose.MenuItem
import org.mozilla.fenix.components.menu.compose.MenuScaffold
import org.mozilla.fenix.components.menu.compose.MenuTextItem
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

internal const val PROTECTION_PANEL_ROUTE = "protection_panel"

@Composable
internal fun ProtectionPanel(
    url: String,
    title: String,
    isSecured: Boolean,
    onTrackerBlockedMenuClick: () -> Unit,
    onClearSiteDataMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            ProtectionPanelHeader(
                url = url,
                title = title,
                isSecured = isSecured,
            )
        },
    ) {
        MenuGroup {
            MenuItem(
                label = "5 Trackers blocked",
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_shield_24),
                onClick = onTrackerBlockedMenuClick,
                afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            SwitchWithLabel(
                label = "Enhanced Tracking Protection ",
                checked = true,
                modifier = Modifier.padding(start = 16.dp, top = 6.dp, end = 9.dp, bottom = 14.dp),
                description = "If something looks broken on this site, try turning off protections.",
                onCheckedChange = {},
            )
        }

        MenuGroup {
            MenuTextItem(
                label = stringResource(id = R.string.clear_site_data),
                onClick = onClearSiteDataMenuClick,
            )
        }
    }
}

@Composable
private fun ProtectionPanelHeader(
    url: String,
    title: String,
    isSecured: Boolean,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(start = 12.dp, end = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(
            modifier = Modifier
                .padding(horizontal = 8.dp)
                .weight(1f),
        ) {
            Text(
                text = title,
                color = FirefoxTheme.colors.textSecondary,
                maxLines = 1,
                style = FirefoxTheme.typography.headline7,
            )

            Text(
                text = url,
                color = FirefoxTheme.colors.textSecondary,
                maxLines = 1,
                style = FirefoxTheme.typography.caption,
            )
        }

        Spacer(modifier = Modifier.width(8.dp))

        Divider(modifier = Modifier.size(width = 2.dp, height = 32.dp))

        Spacer(modifier = Modifier.width(4.dp))

        IconButton(
            onClick = {},
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_lock_20),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconSecondary,
                )

                Text(
                    text = if (isSecured) { "Secured" } else { "" },
                    color = FirefoxTheme.colors.textSecondary,
                    maxLines = 1,
                    style = FirefoxTheme.typography.caption,
                )

                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconSecondary,
                )
            }
        }
    }
}

@LightDarkPreview
@Composable
private fun ProtectionPanelPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            ProtectionPanel(
                url = "https://www.mozilla.org",
                title = "Mozilla",
                isSecured = true,
                onTrackerBlockedMenuClick = {},
                onClearSiteDataMenuClick = {},
            )
        }
    }
}
