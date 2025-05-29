/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.store.TranslationInfo
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

@Suppress("LongParameterList")
@Composable
internal fun MoreSettingsSubmenu(
    isPinned: Boolean,
    isInstallable: Boolean,
    hasExternalApp: Boolean,
    externalAppName: String,
    isReaderViewActive: Boolean,
    isWebCompatReporterSupported: Boolean,
    isWebCompatEnabled: Boolean,
    translationInfo: TranslationInfo,
    onWebCompatReporterClick: () -> Unit,
    onShortcutsMenuClick: () -> Unit,
    onAddToHomeScreenMenuClick: () -> Unit,
    onSaveToCollectionMenuClick: () -> Unit,
    onSaveAsPDFMenuClick: () -> Unit,
    onPrintMenuClick: () -> Unit,
    onOpenInAppMenuClick: () -> Unit,
) {
    Column(
        verticalArrangement = Arrangement.spacedBy(2.dp),
    ) {
        if (translationInfo.isTranslationSupported) {
            TranslationMenuItem(
                translationInfo = translationInfo,
                isReaderViewActive = isReaderViewActive,
            )
        }

        if (isWebCompatReporterSupported) {
            MenuItem(
                label = stringResource(id = R.string.browser_menu_webcompat_reporter),
                beforeIconPainter = painterResource(id = R.drawable.mozac_ic_lightbulb_24),
                state = if (isWebCompatEnabled) MenuItemState.ENABLED else MenuItemState.DISABLED,
                onClick = onWebCompatReporterClick,
            )
        }

        ShortcutsMenuItem(
            isPinned = isPinned,
            onShortcutsMenuClick = onShortcutsMenuClick,
        )

        MenuItem(
            label = if (isInstallable) {
                stringResource(id = R.string.browser_menu_add_app_to_homescreen_2)
            } else {
                stringResource(id = R.string.browser_menu_add_to_homescreen_2)
            },
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_add_to_homescreen_24),
            onClick = onAddToHomeScreenMenuClick,
        )

        MenuItem(
            label = stringResource(id = R.string.browser_menu_save_to_collection),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_collection_24),
            onClick = onSaveToCollectionMenuClick,
        )

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

        MenuItem(
            label = stringResource(id = R.string.browser_menu_save_as_pdf),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_save_file_24),
            onClick = onSaveAsPDFMenuClick,
        )

        MenuItem(
            label = stringResource(id = R.string.browser_menu_print),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_print_24),
            onClick = onPrintMenuClick,
        )
    }
}

@Composable
private fun TranslationMenuItem(
    translationInfo: TranslationInfo,
    isReaderViewActive: Boolean,
) {
    if (translationInfo.isTranslated) {
        val state = if (isReaderViewActive || translationInfo.isPdf) MenuItemState.DISABLED else MenuItemState.ACTIVE
        MenuItem(
            label = stringResource(id = R.string.browser_menu_translated),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_translate_active_24),
            state = state,
            onClick = translationInfo.onTranslatePageMenuClick,
        ) {
            Badge(
                badgeText = translationInfo.translatedLanguage,
                state = state,
                badgeBackgroundColor = FirefoxTheme.colors.badgeActive,
            )
        }
    } else {
        MenuItem(
            label = stringResource(id = R.string.browser_menu_translate_page),
            beforeIconPainter = painterResource(id = R.drawable.mozac_ic_translate_24),
            state = if (isReaderViewActive || translationInfo.isPdf) MenuItemState.DISABLED else MenuItemState.ENABLED,
            onClick = translationInfo.onTranslatePageMenuClick,
        )
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
private fun MoreSettingsSubmenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            MenuGroup {
                MoreSettingsSubmenu(
                    isPinned = true,
                    isInstallable = true,
                    hasExternalApp = true,
                    externalAppName = "Pocket",
                    isReaderViewActive = false,
                    isWebCompatReporterSupported = true,
                    isWebCompatEnabled = true,
                    translationInfo = TranslationInfo(
                        isTranslationSupported = true,
                        isPdf = false,
                        isTranslated = true,
                        translatedLanguage = "English",
                        onTranslatePageMenuClick = {},
                    ),
                    onWebCompatReporterClick = {},
                    onShortcutsMenuClick = {},
                    onAddToHomeScreenMenuClick = {},
                    onSaveToCollectionMenuClick = {},
                    onSaveAsPDFMenuClick = {},
                    onPrintMenuClick = {},
                    onOpenInAppMenuClick = {},
                )
            }
        }
    }
}

@Preview
@Composable
private fun MoreSettingsSubmenuPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            MenuGroup {
                MoreSettingsSubmenu(
                    isPinned = false,
                    isInstallable = true,
                    hasExternalApp = false,
                    externalAppName = "Pocket",
                    isReaderViewActive = false,
                    isWebCompatReporterSupported = true,
                    isWebCompatEnabled = true,
                    translationInfo = TranslationInfo(
                        isTranslationSupported = true,
                        isPdf = false,
                        isTranslated = false,
                        translatedLanguage = "English",
                        onTranslatePageMenuClick = {},
                    ),
                    onWebCompatReporterClick = {},
                    onShortcutsMenuClick = {},
                    onAddToHomeScreenMenuClick = {},
                    onSaveToCollectionMenuClick = {},
                    onSaveAsPDFMenuClick = {},
                    onPrintMenuClick = {},
                    onOpenInAppMenuClick = {},
                )
            }
        }
    }
}
