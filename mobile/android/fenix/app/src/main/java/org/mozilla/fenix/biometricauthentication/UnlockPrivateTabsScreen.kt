/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.biometricauthentication

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.button.TextButton
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

/**
 * A screen allowing users to unlock their private tabs.
 *
 * @param onUnlockClicked Invoked when the user taps the unlock button.
 * @param onLeaveClicked Invoked when the user taps the leave private tabs text.
 */
@Composable
internal fun UnlockPrivateTabsScreen(
    onUnlockClicked: () -> Unit,
    onLeaveClicked: () -> Unit,
) {
    Column(
        modifier = Modifier.fillMaxSize()
            .background(FirefoxTheme.colors.layer1)
            .padding(bottom = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Spacer(modifier = Modifier.height(32.dp))

        Header()

        Footer(onUnlockClicked, onLeaveClicked)

        LaunchedEffect(Unit) {
            // Record telemetry event here as
            // part of https://mozilla-hub.atlassian.net/browse/FXDROID-3385
        }
    }
}

@Composable
private fun Header() {
    Column(
        modifier = Modifier.padding(horizontal = 16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Image(
            painter = painterResource(id = R.drawable.ic_pbm_firefox_logo),
            contentDescription = null, // decorative only.
            modifier = Modifier.padding(32.dp),
        )

        Spacer(modifier = Modifier.height(24.dp))

        Text(
            text = stringResource(id = R.string.pbm_authentication_unlock_private_tabs),
            color = FirefoxTheme.colors.textPrimary,
            textAlign = TextAlign.Center,
            style = FirefoxTheme.typography.headline6,
            maxLines = 1,
        )
    }
}

@Composable
private fun Footer(onUnlockClicked: () -> Unit, onLeaveClicked: () -> Unit) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier.padding(horizontal = 16.dp),
    ) {
        PrimaryButton(
            onClick = onUnlockClicked,
            icon = painterResource(id = R.drawable.ic_lock),
            text = stringResource(id = R.string.pbm_authentication_unlock),
        )

        Spacer(modifier = Modifier.height(8.dp))

        TextButton(
            text = stringResource(R.string.pbm_authentication_leave_private_tabs),
            onClick = onLeaveClicked,
            upperCaseText = false,
        )
    }
}

@FlexibleWindowLightDarkPreview
@Composable
private fun UnlockPrivateTabsPreview() {
    FirefoxTheme(Theme.Private) {
        UnlockPrivateTabsScreen(
            onUnlockClicked = {},
            onLeaveClicked = {},
        )
    }
}
