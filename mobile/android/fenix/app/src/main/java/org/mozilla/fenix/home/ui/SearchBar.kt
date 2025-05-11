/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.PreviewLightDark
import mozilla.components.compose.browser.toolbar.BrowserToolbarDefaults
import mozilla.components.compose.browser.toolbar.HomepageDisplayToolbar
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * Search bar.
 *
 * @param onClick Invoked when the user clicks on the search bar text.
 */
@Composable
internal fun SearchBar(
    onClick: () -> Unit,
) {
    HomepageDisplayToolbar(
        url = stringResource(R.string.search_hint),
        colors = BrowserToolbarDefaults.colors().displayToolbarColors.copy(background = Color.Transparent),
        onUrlClicked = onClick,
    )
}

@Composable
@PreviewLightDark
private fun SearchBarPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer1),
        ) {
            SearchBar(
                onClick = {},
            )
        }
    }
}
