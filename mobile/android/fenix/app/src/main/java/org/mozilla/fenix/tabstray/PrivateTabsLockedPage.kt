/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.button.PrimaryButton
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * UI for displaying the Unlock Private Tabs Page in the Tabs Tray.
 *
 * @param onUnlockClicked Invoked when the user taps the unlock button.
 */
@Composable
internal fun PrivateTabsLockedPage(onUnlockClicked: () -> Unit) {
    Column(
        modifier = Modifier.fillMaxSize()
            .padding(horizontal = 16.dp)
            .padding(bottom = 32.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Text(
            text = stringResource(id = R.string.pbm_authentication_unlock_private_tabs),
            color = FirefoxTheme.colors.textPrimary,
            textAlign = TextAlign.Center,
            style = FirefoxTheme.typography.body1,
        )

        PrimaryButton(
            onClick = onUnlockClicked,
            icon = painterResource(id = R.drawable.ic_lock),
            text = stringResource(id = R.string.pbm_authentication_unlock),
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun PrivateTabsLockedPagePreview() {
    FirefoxTheme(Theme.Private) {
        PrivateTabsLockedPage(
            onUnlockClicked = {},
        )
    }
}
