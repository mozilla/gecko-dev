/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen

import androidx.compose.foundation.layout.RowScope
import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.menu.DropdownMenu
import mozilla.components.compose.base.menu.MenuItem
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * The toolbar for the downloads screen.
 * It displays the title, navigation icon, and an optional overflow menu.
 *
 * @param backgroundColor - The background color for the TopAppBar.
 * @param title - The title to be displayed in the center of the TopAppBar.
 * @param navigationIcon - The navigation icon displayed at the start of the TopAppBar.
 * @param actions - The actions displayed at the end of the TopAppBar.
 */
@Composable
internal fun Toolbar(
    backgroundColor: Color,
    title: @Composable () -> Unit,
    navigationIcon: @Composable (() -> Unit),
    actions: @Composable RowScope.() -> Unit = {},
) {
    TopAppBar(
        backgroundColor = backgroundColor,
        title = title,
        navigationIcon = navigationIcon,
        actions = actions,
    )
}

/**
 * A dropdown menu for the downloads screen.
 *
 * @param showMenu `true` to display the menu, `false` otherwise.
 * @param menuItems The list of [MenuItem] to display in the dropdown menu.
 * @param onDismissRequest A callback that is invoked when the user attempts to dismiss the menu.
 */
@Composable
fun DownloadsOverflowMenu(
    showMenu: Boolean,
    menuItems: List<MenuItem>,
    onDismissRequest: () -> Unit,
) {
    DropdownMenu(
        menuItems = menuItems,
        expanded = showMenu,
        onDismissRequest = onDismissRequest,
    )
}

/**
 * @property title The title text to display in the Toolbar.
 * @property backgroundColor The background color of the Toolbar.
 * @property textColor The color of the text (title) in the Toolbar.
 * @property iconColor The color of the icons (e.g., navigation icon, overflow icon) in the Toolbar.
 */
data class ToolbarConfig(
    val title: String,
    val backgroundColor: Color,
    val textColor: Color,
    val iconColor: Color,
)

@Composable
@FlexibleWindowLightDarkPreview
private fun ToolbarPreview() {
    FirefoxTheme {
        Toolbar(
            backgroundColor = FirefoxTheme.colors.layerAccent,
            title = {
                Text(
                    color = FirefoxTheme.colors.textOnColorPrimary,
                    style = FirefoxTheme.typography.headline6,
                    text = stringResource(
                        R.string.download_multi_select_title,
                        1,
                    ),
                )
            },
            navigationIcon = {
                IconButton(onClick = {}) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_back_24),
                        contentDescription = stringResource(R.string.download_navigate_back_description),
                        tint = FirefoxTheme.colors.iconPrimary,
                    )
                }
            },
            actions = {
                IconButton(onClick = {}) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                        contentDescription = stringResource(
                            R.string.content_description_menu,
                        ),
                        tint = FirefoxTheme.colors.iconOnColor,
                    )
                }
            },
        )
    }
}
