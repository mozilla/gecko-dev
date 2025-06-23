/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.biometricauthentication

import android.content.res.Configuration
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.button.PrimaryButton
import mozilla.components.compose.base.button.TextButton
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.isLargeWindow
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

private const val FILL_WIDTH_LARGE_WINDOW = 0.5f
private const val FILL_WIDTH_DEFAULT = 1.0f
private const val PHONE_WIDTH = 400
private const val PHONE_HEIGHT = 640
private const val TABLET_WIDTH = 700
private const val TABLET_HEIGHT = 1280

/**
 * A screen allowing users to unlock their private tabs.
 *
 * @param onUnlockClicked Invoked when the user taps the unlock button.
 * @param onLeaveClicked Invoked when the user taps the leave private tabs text.
 * @param showNegativeButton To check if we display the negative button.
 */
@Composable
internal fun UnlockPrivateTabsScreen(
    onUnlockClicked: () -> Unit,
    onLeaveClicked: () -> Unit,
    showNegativeButton: Boolean,
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(FirefoxTheme.colors.layer1)
            .padding(bottom = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
        Spacer(modifier = Modifier.height(32.dp))

        Header()

        Footer(onUnlockClicked, onLeaveClicked, showNegativeButton)

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
private fun Footer(onUnlockClicked: () -> Unit, onLeaveClicked: () -> Unit, showNegativeButton: Boolean) {
    val fillWidthFraction = if (LocalContext.current.isLargeWindow()) {
        FILL_WIDTH_LARGE_WINDOW
    } else {
        FILL_WIDTH_DEFAULT
    }

    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier
            .padding(horizontal = 16.dp)
            .fillMaxWidth(fillWidthFraction),
    ) {
        PrimaryButton(
            text = stringResource(id = R.string.pbm_authentication_unlock),
            modifier = Modifier.fillMaxWidth(),
            onClick = onUnlockClicked,
        )

        Spacer(modifier = Modifier.height(8.dp))

        if (showNegativeButton) {
            TextButton(
                text = stringResource(R.string.pbm_authentication_leave_private_tabs),
                onClick = onLeaveClicked,
                textColor = FirefoxTheme.colors.textPrimary,
                upperCaseText = false,
            )
        }
    }
}

@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO, widthDp = PHONE_WIDTH, heightDp = PHONE_HEIGHT)
@Composable
private fun ScreenPreviewLightPhone() = ScreenPreview(Theme.Light)

@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES, widthDp = PHONE_WIDTH, heightDp = PHONE_HEIGHT)
@Composable
private fun ScreenPreviewDarkPhone() = ScreenPreview(Theme.Dark)

@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES, widthDp = PHONE_WIDTH, heightDp = PHONE_HEIGHT)
@Composable
private fun ScreenPreviewPrivatePhone() = ScreenPreview(Theme.Private)

@Preview(uiMode = Configuration.UI_MODE_NIGHT_NO, widthDp = TABLET_WIDTH, heightDp = TABLET_HEIGHT)
@Composable
private fun ScreenPreviewLightTablet() = ScreenPreview(Theme.Light)

@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES, widthDp = TABLET_WIDTH, heightDp = TABLET_HEIGHT)
@Composable
private fun ScreenPreviewDarkTablet() = ScreenPreview(Theme.Dark)

@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES, widthDp = TABLET_WIDTH, heightDp = TABLET_HEIGHT)
@Composable
private fun ScreenPreviewPrivateTablet() = ScreenPreview(Theme.Private)

@Composable
private fun ScreenPreview(theme: Theme) {
    FirefoxTheme(theme) {
        UnlockPrivateTabsScreen(
            onUnlockClicked = {},
            onLeaveClicked = {},
            showNegativeButton = true,
        )
    }
}
