/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import android.graphics.BitmapFactory
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.header.SubmenuHeader
import org.mozilla.fenix.components.menu.store.WebExtensionMenuItem
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

@Suppress("LongParameterList", "LongMethod")
@Composable
internal fun ToolsSubmenu(
    isPdf: Boolean,
    isReportSiteIssueSupported: Boolean,
    webExtensionMenuItems: List<WebExtensionMenuItem.WebExtensionPageMenuItem>,
    isReaderable: Boolean,
    isReaderViewActive: Boolean,
    isTranslated: Boolean,
    isTranslationSupported: Boolean,
    hasExternalApp: Boolean,
    externalAppName: String,
    translatedLanguage: String,
    onBackButtonClick: () -> Unit,
    onReaderViewMenuClick: () -> Unit,
    onCustomizeReaderViewMenuClick: () -> Unit,
    onTranslatePageMenuClick: () -> Unit,
    onPrintMenuClick: () -> Unit,
    onShareMenuClick: () -> Unit,
    onOpenInAppMenuClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            SubmenuHeader(
                header = stringResource(id = R.string.browser_menu_tools),
                backButtonContentDescription = stringResource(
                    id = R.string.browser_menu_back_button_content_description,
                ),
                onClick = onBackButtonClick,
            )
        },
    ) {
        MenuGroup {
            ReaderViewMenuItem(
                isReaderable = isReaderable,
                isReaderViewActive = isReaderViewActive,
                onClick = onReaderViewMenuClick,
            )

            if (isReaderViewActive) {
                Divider(color = FirefoxTheme.colors.borderSecondary)

                MenuItem(
                    label = stringResource(id = R.string.browser_menu_customize_reader_view_2),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_reader_view_customize_24),
                    onClick = onCustomizeReaderViewMenuClick,
                )
            }

            if (isTranslationSupported) {
                Divider(color = FirefoxTheme.colors.borderSecondary)

                TranslationMenuItem(
                    isPdf = isPdf,
                    isTranslated = isTranslated,
                    isReaderViewActive = isReaderViewActive,
                    translatedLanguage = translatedLanguage,
                    onClick = onTranslatePageMenuClick,
                )
            }

            if (webExtensionMenuItems.isNotEmpty() && isReportSiteIssueSupported) {
                Divider(color = FirefoxTheme.colors.borderSecondary)

                for (webExtensionMenuItem in webExtensionMenuItems) {
                    WebExtensionMenuItem(
                        label = webExtensionMenuItem.label,
                        iconPainter = webExtensionMenuItem.icon?.let { icon ->
                            BitmapPainter(image = icon.asImageBitmap())
                        } ?: painterResource(R.drawable.mozac_ic_web_extension_default_icon),
                        iconTint = FirefoxTheme.colors.iconSecondary,
                        enabled = webExtensionMenuItem.enabled,
                        badgeText = webExtensionMenuItem.badgeText,
                        badgeTextColor = webExtensionMenuItem.badgeTextColor,
                        badgeBackgroundColor = webExtensionMenuItem.badgeBackgroundColor,
                        onClick = webExtensionMenuItem.onClick,
                    )
                }
            }
        }

        MenuGroup {
            MenuItem(
                label = stringResource(id = R.string.browser_menu_print),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_print_24),
                onClick = onPrintMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            MenuItem(
                label = stringResource(id = R.string.browser_menu_share_2),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_share_android_24),
                onClick = onShareMenuClick,
            )

            Divider(color = FirefoxTheme.colors.borderSecondary)

            if (hasExternalApp) {
                MenuItem(
                    label = if (externalAppName != "") {
                        stringResource(id = R.string.browser_menu_open_in_fenix, externalAppName)
                    } else {
                        stringResource(id = R.string.browser_menu_open_app_link)
                    },
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_more_grid_24),
                    state = MenuItemState.ENABLED,
                    onClick = onOpenInAppMenuClick,
                )
            } else {
                MenuItem(
                    label = stringResource(id = R.string.browser_menu_open_app_link),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_more_grid_24),
                    state = MenuItemState.DISABLED,
                )
            }
        }
    }
}

@Composable
private fun ReaderViewMenuItem(
    isReaderable: Boolean,
    isReaderViewActive: Boolean,
    onClick: () -> Unit,
) {
    if (isReaderViewActive) {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_turn_off_reader_view),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_reader_view_fill_24),
            state = MenuItemState.ACTIVE,
            onClick = onClick,
        )
    } else {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_turn_on_reader_view),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_reader_view_24),
            state = if (isReaderable) MenuItemState.ENABLED else MenuItemState.DISABLED,
            onClick = onClick,
        )
    }
}

@Composable
private fun TranslationMenuItem(
    isPdf: Boolean,
    isTranslated: Boolean,
    isReaderViewActive: Boolean,
    translatedLanguage: String,
    onClick: () -> Unit,
) {
    if (isTranslated) {
        MenuItem(
            label = stringResource(
                id = R.string.browser_menu_translated_to,
                translatedLanguage,
            ),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_translate_active_24),
            state = if (isReaderViewActive || isPdf) MenuItemState.DISABLED else MenuItemState.ACTIVE,
            onClick = onClick,
        )
    } else {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_translate_page),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_translate_24),
            state = if (isReaderViewActive || isPdf) MenuItemState.DISABLED else MenuItemState.ENABLED,
            onClick = onClick,
        )
    }
}

@LightDarkPreview
@Composable
private fun ToolsSubmenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            ToolsSubmenu(
                isPdf = false,
                isReportSiteIssueSupported = false,
                webExtensionMenuItems = listOf(
                    WebExtensionMenuItem.WebExtensionPageMenuItem(
                        label = "label",
                        enabled = true,
                        icon = BitmapFactory.decodeResource(
                            LocalContext.current.resources,
                            R.drawable.mozac_ic_web_extension_default_icon,
                        ),
                        badgeText = "1",
                        badgeTextColor = Color.White.toArgb(),
                        badgeBackgroundColor = Color.Gray.toArgb(),
                        onClick = {
                        },
                    ),
                ),
                isReaderable = true,
                isReaderViewActive = false,
                isTranslated = false,
                isTranslationSupported = false,
                hasExternalApp = true,
                externalAppName = "Pocket",
                translatedLanguage = "",
                onBackButtonClick = {},
                onReaderViewMenuClick = {},
                onCustomizeReaderViewMenuClick = {},
                onTranslatePageMenuClick = {},
                onPrintMenuClick = {},
                onShareMenuClick = {},
                onOpenInAppMenuClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun ToolsSubmenuPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            ToolsSubmenu(
                isPdf = false,
                isReportSiteIssueSupported = true,
                webExtensionMenuItems = listOf(
                    WebExtensionMenuItem.WebExtensionPageMenuItem(
                        label = "label",
                        enabled = true,
                        icon = BitmapFactory.decodeResource(
                            LocalContext.current.resources,
                            R.drawable.mozac_ic_web_extension_default_icon,
                        ),
                        badgeText = "1",
                        badgeTextColor = Color.White.toArgb(),
                        badgeBackgroundColor = Color.Gray.toArgb(),
                        onClick = {
                        },
                    ),
                ),
                isReaderable = true,
                isReaderViewActive = false,
                isTranslated = false,
                isTranslationSupported = true,
                hasExternalApp = true,
                externalAppName = "Pocket",
                translatedLanguage = "",
                onBackButtonClick = {},
                onReaderViewMenuClick = {},
                onCustomizeReaderViewMenuClick = {},
                onTranslatePageMenuClick = {},
                onPrintMenuClick = {},
                onShareMenuClick = {},
                onOpenInAppMenuClick = {},
            )
        }
    }
}
