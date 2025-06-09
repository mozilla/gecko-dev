/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import mozilla.components.compose.base.Divider
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.header.SubmenuHeader
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

@Suppress("LongParameterList")
@Composable
internal fun SaveSubmenu(
    isPinned: Boolean,
    isInstallable: Boolean,
    onBackButtonClick: () -> Unit,
    onShortcutsMenuClick: () -> Unit,
    onAddToHomeScreenMenuClick: () -> Unit,
    onSaveToCollectionMenuClick: () -> Unit,
    onSaveAsPDFMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            SubmenuHeader(
                header = stringResource(id = R.string.browser_menu_save),
                backButtonContentDescription = stringResource(
                    id = R.string.browser_menu_back_button_content_description,
                ),
                onClick = onBackButtonClick,
            )
        },
    ) {
        MenuGroup {
            ShortcutsMenuItem(
                isPinned = isPinned,
                onShortcutsMenuClick = onShortcutsMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = if (isInstallable) {
                    stringResource(id = R.string.browser_menu_add_app_to_homescreen_2)
                } else {
                    stringResource(id = R.string.browser_menu_add_to_homescreen_2)
                },
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_add_to_homescreen_24),
                onClick = onAddToHomeScreenMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_save_to_collection),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_collection_24),
                onClick = onSaveToCollectionMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_save_as_pdf),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_save_file_24),
                onClick = onSaveAsPDFMenuClick,
            )
        }
    }
}

@Composable
private fun ShortcutsMenuItem(
    isPinned: Boolean,
    onShortcutsMenuClick: () -> Unit,
) {
    MenuItem(
        label = if (isPinned) {
            stringResource(id = R.string.browser_menu_remove_from_shortcuts)
        } else {
            stringResource(id = R.string.browser_menu_add_to_shortcuts)
        },
        beforeIconPainter = if (isPinned) {
            painterResource(id = R.drawable.mozac_ic_pin_slash_fill_24)
        } else {
            painterResource(id = R.drawable.mozac_ic_pin_24)
        },
        state = if (isPinned) {
            MenuItemState.ACTIVE
        } else {
            MenuItemState.ENABLED
        },
        onClick = onShortcutsMenuClick,
    )
}

@PreviewLightDark
@Composable
private fun SaveSubmenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            SaveSubmenu(
                isPinned = false,
                isInstallable = false,
                onBackButtonClick = {},
                onShortcutsMenuClick = {},
                onAddToHomeScreenMenuClick = {},
                onSaveToCollectionMenuClick = {},
                onSaveAsPDFMenuClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun SaveSubmenuPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            SaveSubmenu(
                isPinned = false,
                isInstallable = true,
                onBackButtonClick = {},
                onShortcutsMenuClick = {},
                onAddToHomeScreenMenuClick = {},
                onSaveToCollectionMenuClick = {},
                onSaveAsPDFMenuClick = {},
            )
        }
    }
}
