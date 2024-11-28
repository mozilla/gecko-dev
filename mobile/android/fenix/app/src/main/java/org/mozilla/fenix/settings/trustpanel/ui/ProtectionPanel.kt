/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.trustpanel.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.MenuGroup
import org.mozilla.fenix.components.menu.compose.MenuScaffold
import org.mozilla.fenix.components.menu.compose.MenuTextItem
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

internal const val PROTECTION_PANEL_ROUTE = "protection_panel"

@Composable
internal fun ProtectionPanel(
    onClearSiteDataMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {},
    ) {
        MenuGroup {
            MenuTextItem(
                label = stringResource(id = R.string.clear_site_data),
                onClick = onClearSiteDataMenuClick,
            )
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
                onClearSiteDataMenuClick = {},
            )
        }
    }
}
