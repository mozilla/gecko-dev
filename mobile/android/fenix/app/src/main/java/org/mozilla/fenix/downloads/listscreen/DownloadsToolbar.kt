/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.downloads.listscreen

import androidx.compose.material.Icon
import androidx.compose.material.IconButton
import androidx.compose.material.Text
import androidx.compose.material.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.menu.DropdownMenu
import org.mozilla.fenix.compose.menu.MenuItem
import org.mozilla.fenix.downloads.listscreen.store.CreatedTime
import org.mozilla.fenix.downloads.listscreen.store.DownloadUIState
import org.mozilla.fenix.downloads.listscreen.store.FileItem
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.compose.text.Text as Text

/**
 * Composable function that represents the toolbar for the downloads screen.
 *
 * @param mode The current mode of the downloads screen.
 * @param overflowMenuItems The list of [MenuItem] to display in the overflow menu (if in editing mode).
 * @param toolbarConfig Configuration for the toolbar's appearance (title, colors).
 * @param onNavigationIconClick Callback for the back button click.
 */
@Composable
fun Toolbar(
    mode: DownloadUIState.Mode,
    overflowMenuItems: List<MenuItem>,
    toolbarConfig: ToolbarConfig,
    onNavigationIconClick: () -> Unit,
) {
    var showMenu by remember { mutableStateOf(false) }

    TopAppBar(
        backgroundColor = toolbarConfig.backgroundColor,
        title = {
            Text(
                color = toolbarConfig.textColor,
                style = FirefoxTheme.typography.headline6,
                text = toolbarConfig.title,
            )
        },
        navigationIcon = {
            IconButton(onClick = onNavigationIconClick) {
                Icon(
                    painter = painterResource(R.drawable.mozac_ic_back_24),
                    contentDescription = stringResource(R.string.preference_doh_up_description),
                    tint = toolbarConfig.iconColor,
                )
            }
        },
        actions = {
            if (mode is DownloadUIState.Mode.Editing) {
                IconButton(onClick = { showMenu = true }) {
                    Icon(
                        painter = painterResource(R.drawable.mozac_ic_ellipsis_vertical_24),
                        contentDescription = stringResource(
                            R.string.content_description_menu,
                        ),
                        tint = toolbarConfig.iconColor,
                    )
                }

                DownloadsOverflowMenu(
                    showMenu = showMenu,
                    menuItems = overflowMenuItems,
                    onDismissRequest = { showMenu = false },
                )
            }
        },
    )
}

@Composable
private fun DownloadsOverflowMenu(
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
            mode = DownloadUIState.Mode.Editing(
                setOf(
                    FileItem(
                        id = "3",
                        fileName = "File 3",
                        url = "https://example.com/file3",
                        formattedSize = "3.4 MB",
                        displayedShortUrl = "example.com",
                        contentType = "application/zip",
                        status = DownloadState.Status.COMPLETED,
                        filePath = "/path/to/file1",
                        createdTime = CreatedTime.OLDER,
                    ),
                ),
            ),
            overflowMenuItems = listOf(
                MenuItem.TextItem(
                    text = Text.Resource(R.string.download_select_all_items),
                    level = MenuItem.FixedItem.Level.Default,
                    onClick = { },
                ),
                MenuItem.TextItem(
                    text = Text.Resource(R.string.download_delete_item),
                    level = MenuItem.FixedItem.Level.Critical,
                    onClick = { },
                ),
            ),
            toolbarConfig = ToolbarConfig(
                title = stringResource(
                    R.string.download_multi_select_title,
                    1,
                ),
                backgroundColor = FirefoxTheme.colors.layerAccent,
                textColor = FirefoxTheme.colors.textOnColorPrimary,
                iconColor = FirefoxTheme.colors.iconOnColor,
            ),
            onNavigationIconClick = {},
        )
    }
}
