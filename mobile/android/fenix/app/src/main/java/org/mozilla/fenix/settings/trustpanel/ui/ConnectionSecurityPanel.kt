/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.annotation.LightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.MenuGroup
import org.mozilla.fenix.components.menu.compose.MenuItem
import org.mozilla.fenix.components.menu.compose.MenuScaffold
import org.mozilla.fenix.components.menu.compose.MenuTextItem
import org.mozilla.fenix.components.menu.compose.header.SubmenuHeader
import org.mozilla.fenix.theme.FirefoxTheme

internal const val CONNECTION_SECURITY_PANEL_ROUTE = "connection_security_panel"

@Composable
internal fun ConnectionSecurityPanel(
    title: String,
    isSecured: Boolean,
    certificateName: String,
    onBackButtonClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            SubmenuHeader(
                header = title,
                onClick = onBackButtonClick,
            )
        },
    ) {
        Column {
            MenuGroup {
                MenuItem(
                    label = if (isSecured) {
                        stringResource(id = R.string.connection_security_panel_secure)
                    } else {
                        stringResource(id = R.string.connection_security_panel_not_secure)
                    },
                    beforeIconPainter = if (isSecured) {
                        painterResource(id = R.drawable.mozac_ic_lock_24)
                    } else {
                        painterResource(id = R.drawable.mozac_ic_lock_slash_24)
                    },
                )

                if (certificateName.isNotEmpty()) {
                    Divider(color = FirefoxTheme.colors.borderSecondary)

                    MenuTextItem(
                        label = stringResource(
                            id = R.string.connection_security_panel_verified_by,
                            certificateName,
                        ),
                    )
                }
            }
        }
    }
}

@LightDarkPreview
@Composable
private fun TrackersBlockedPanelPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            ConnectionSecurityPanel(
                title = "Mozilla",
                isSecured = true,
                certificateName = "Let's Encrypt",
                onBackButtonClick = {},
            )
        }
    }
}
