/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.ui

import android.graphics.Bitmap
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.annotation.LightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.MenuGroup
import org.mozilla.fenix.components.menu.compose.MenuItem
import org.mozilla.fenix.components.menu.compose.MenuScaffold
import org.mozilla.fenix.components.menu.compose.MenuTextItem
import org.mozilla.fenix.compose.LinkText
import org.mozilla.fenix.compose.LinkTextState
import org.mozilla.fenix.compose.SwitchWithLabel
import org.mozilla.fenix.theme.FirefoxTheme

internal const val PROTECTION_PANEL_ROUTE = "protection_panel"

private val ROUNDED_CORNER_SHAPE = RoundedCornerShape(4.dp)

@Suppress("LongParameterList", "LongMethod")
@Composable
internal fun ProtectionPanel(
    url: String,
    title: String,
    icon: Bitmap?,
    isSecured: Boolean,
    isTrackingProtectionEnabled: Boolean,
    numberOfTrackersBlocked: Int,
    onTrackerBlockedMenuClick: () -> Unit,
    onTrackingProtectionToggleClick: () -> Unit,
    onClearSiteDataMenuClick: () -> Unit,
    onConnectionSecurityClick: () -> Unit,
    onPrivacySecuritySettingsClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            ProtectionPanelHeader(
                url = url,
                title = title,
                icon = icon,
                isSecured = isSecured,
                onConnectionSecurityClick = onConnectionSecurityClick,
            )
        },
    ) {
        MenuGroup {
            ProtectionPanelBanner(
                isSecured = isSecured,
                isTrackingProtectionEnabled = isTrackingProtectionEnabled,
            )

            if (numberOfTrackersBlocked == 0) {
                MenuItem(
                    label = stringResource(id = R.string.protection_panel_no_trackers_blocked),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_shield_24),
                )
            } else {
                MenuItem(
                    label = stringResource(
                        id = R.string.protection_panel_num_trackers_blocked,
                        numberOfTrackersBlocked,
                    ),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_shield_24),
                    onClick = onTrackerBlockedMenuClick,
                    afterIconPainter = painterResource(id = R.drawable.mozac_ic_chevron_right_24),
                )
            }

            Divider(color = FirefoxTheme.colors.borderSecondary)

            SwitchWithLabel(
                label = stringResource(id = R.string.protection_panel_etp_toggle_label),
                checked = isTrackingProtectionEnabled,
                modifier = Modifier.padding(start = 16.dp, top = 6.dp, end = 9.dp, bottom = 14.dp),
                description = if (isTrackingProtectionEnabled) {
                    stringResource(id = R.string.protection_panel_etp_toggle_enabled_description)
                } else {
                    stringResource(id = R.string.protection_panel_etp_toggle_disabled_description)
                },
                onCheckedChange = { onTrackingProtectionToggleClick() },
            )
        }

        MenuGroup {
            MenuTextItem(
                label = stringResource(id = R.string.clear_site_data),
                onClick = onClearSiteDataMenuClick,
            )
        }

        LinkText(
            text = stringResource(id = R.string.protection_panel_privacy_and_security_settings),
            linkTextStates = listOf(
                LinkTextState(
                    text = stringResource(id = R.string.protection_panel_privacy_and_security_settings),
                    url = "",
                    onClick = { onPrivacySecuritySettingsClick() },
                ),
            ),
            linkTextColor = FirefoxTheme.colors.textAccent,
            linkTextDecoration = TextDecoration.Underline,
        )
    }
}

@Composable
private fun ProtectionPanelBanner(
    isSecured: Boolean,
    isTrackingProtectionEnabled: Boolean,
) {
    val backgroundColor: Color
    val imageId: Int
    val title: String
    val description: String

    if (!isSecured) {
        backgroundColor = FirefoxTheme.colors.layerCritical
        imageId = R.drawable.protection_panel_not_secure
        title = stringResource(id = R.string.protection_panel_banner_not_secure_title)
        description = stringResource(id = R.string.protection_panel_banner_not_secure_description)
    } else if (!isTrackingProtectionEnabled) {
        backgroundColor = FirefoxTheme.colors.layer3
        imageId = R.drawable.protection_panel_not_protected
        title = stringResource(id = R.string.protection_panel_banner_not_protected_title)
        description = stringResource(
            id = R.string.protection_panel_banner_not_protected_description,
            stringResource(id = R.string.app_name),
        )
    } else {
        backgroundColor = FirefoxTheme.colors.layerAccentNonOpaque
        imageId = R.drawable.protection_panel_protected
        title = stringResource(
            id = R.string.protection_panel_banner_protected_title,
            stringResource(id = R.string.app_name),
        )
        description = stringResource(id = R.string.protection_panel_banner_protected_description)
    }

    Card(
        modifier = Modifier
            .padding(start = 8.dp, top = 8.dp, end = 8.dp)
            .fillMaxWidth(),
        backgroundColor = backgroundColor,
        elevation = 0.dp,
        shape = ROUNDED_CORNER_SHAPE,
    ) {
        Row(
            modifier = Modifier.padding(start = 12.dp, top = 16.dp, end = 16.dp, bottom = 16.dp),
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Image(
                modifier = Modifier.size(90.dp),
                painter = painterResource(id = imageId),
                contentDescription = null,
            )

            Column(
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text(
                    text = title,
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.headline7,
                )

                Text(
                    text = description,
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.body2,
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
                icon = null,
                isSecured = true,
                isTrackingProtectionEnabled = true,
                numberOfTrackersBlocked = 5,
                onTrackerBlockedMenuClick = {},
                onTrackingProtectionToggleClick = {},
                onClearSiteDataMenuClick = {},
                onConnectionSecurityClick = {},
                onPrivacySecuritySettingsClick = {},
            )
        }
    }
}
