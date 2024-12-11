/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose

import android.graphics.BitmapFactory
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.heading
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.annotation.LightDarkPreview
import mozilla.components.feature.addons.Addon
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.header.SubmenuHeader
import org.mozilla.fenix.components.menu.store.WebExtensionMenuItem
import org.mozilla.fenix.compose.Divider
import org.mozilla.fenix.compose.list.TextListItem
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

@Suppress("LongParameterList", "LongMethod")
@Composable
internal fun ExtensionsSubmenu(
    recommendedAddons: List<Addon>,
    webExtensionMenuItems: List<WebExtensionMenuItem>,
    showExtensionsOnboarding: Boolean,
    showDisabledExtensionsOnboarding: Boolean,
    showManageExtensions: Boolean,
    addonInstallationInProgress: Addon?,
    onBackButtonClick: () -> Unit,
    onExtensionsLearnMoreClick: () -> Unit,
    onManageExtensionsMenuClick: () -> Unit,
    onAddonClick: (Addon) -> Unit,
    onInstallAddonClick: (Addon) -> Unit,
    onDiscoverMoreExtensionsMenuClick: () -> Unit,
    webExtensionMenuItemClick: () -> Unit,
) {
    MenuScaffold(
        header = {
            SubmenuHeader(
                header = stringResource(id = R.string.browser_menu_extensions),
                backButtonContentDescription = stringResource(R.string.browser_menu_back_button_content_description),
                onClick = onBackButtonClick,
            )
        },
    ) {
        if (showExtensionsOnboarding || showDisabledExtensionsOnboarding) {
            ExtensionsSubmenuBanner(
                title = if (showExtensionsOnboarding) {
                    stringResource(
                        R.string.browser_menu_extensions_banner_onboarding_header,
                        stringResource(R.string.app_name),
                    )
                } else {
                    stringResource(R.string.browser_menu_disabled_extensions_banner_onboarding_header)
                },
                description = if (showExtensionsOnboarding) {
                    stringResource(
                        R.string.browser_menu_extensions_banner_onboarding_body,
                        stringResource(R.string.app_name),
                    )
                } else {
                    stringResource(
                        R.string.browser_menu_disabled_extensions_banner_onboarding_body,
                        stringResource(R.string.browser_menu_manage_extensions),
                    )
                },
                linkText = stringResource(R.string.browser_menu_extensions_banner_learn_more),
                onClick = onExtensionsLearnMoreClick,
            )
        }

        if (webExtensionMenuItems.isNotEmpty()) {
            MenuGroup {
                for (webExtensionMenuItem in webExtensionMenuItems) {
                    if (webExtensionMenuItem != webExtensionMenuItems[0]) {
                        Divider(color = FirefoxTheme.colors.borderSecondary)
                    }

                    WebExtensionMenuItem(
                        label = webExtensionMenuItem.label,
                        iconPainter = webExtensionMenuItem.icon?.let { icon ->
                            BitmapPainter(image = icon.asImageBitmap())
                        } ?: painterResource(R.drawable.mozac_ic_web_extension_default_icon),
                        enabled = webExtensionMenuItem.enabled,
                        badgeText = webExtensionMenuItem.badgeText,
                        badgeTextColor = webExtensionMenuItem.badgeTextColor,
                        badgeBackgroundColor = webExtensionMenuItem.badgeBackgroundColor,
                        onClick = {
                            webExtensionMenuItemClick()
                            webExtensionMenuItem.onClick()
                        },
                    )
                }
            }
        }

        if (showManageExtensions) {
            MenuGroup {
                MenuItem(
                    label = stringResource(id = R.string.browser_menu_manage_extensions),
                    beforeIconPainter = painterResource(id = R.drawable.mozac_ic_extension_cog_24),
                    onClick = onManageExtensionsMenuClick,
                )
            }
        }

        RecommendedAddons(
            recommendedAddons = recommendedAddons,
            addonInstallationInProgress = addonInstallationInProgress,
            onAddonClick = onAddonClick,
            onInstallAddonClick = onInstallAddonClick,
            onDiscoverMoreExtensionsMenuClick = onDiscoverMoreExtensionsMenuClick,
        )
    }
}

@Composable
private fun RecommendedAddons(
    recommendedAddons: List<Addon>,
    addonInstallationInProgress: Addon?,
    onAddonClick: (Addon) -> Unit,
    onInstallAddonClick: (Addon) -> Unit,
    onDiscoverMoreExtensionsMenuClick: () -> Unit,
) {
    Column {
        if (recommendedAddons.isNotEmpty()) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                val recommendedSectionContentDescription =
                    stringResource(id = R.string.browser_menu_recommended_section_content_description)
                Text(
                    text = stringResource(id = R.string.mozac_feature_addons_recommended_section),
                    modifier = Modifier
                        .weight(1f)
                        .semantics {
                            heading()
                            this.contentDescription = recommendedSectionContentDescription
                        },
                    color = FirefoxTheme.colors.textSecondary,
                    style = FirefoxTheme.typography.subtitle2,
                )
            }

            Spacer(modifier = Modifier.height(4.dp))
        }

        MenuGroup {
            for (addon in recommendedAddons) {
                AddonMenuItem(
                    addon = addon,
                    addonInstallationInProgress = addonInstallationInProgress,
                    onClick = { onAddonClick(addon) },
                    onIconClick = { onInstallAddonClick(addon) },
                )

                Divider(color = FirefoxTheme.colors.borderSecondary)
            }

            TextListItem(
                label = stringResource(id = R.string.browser_menu_discover_more_extensions),
                modifier = Modifier.semantics {
                    this.role = Role.Button
                },
                onClick = onDiscoverMoreExtensionsMenuClick,
                iconPainter = painterResource(R.drawable.mozac_ic_external_link_24),
                iconTint = FirefoxTheme.colors.iconSecondary,
            )
        }
    }
}

@LightDarkPreview
@Composable
private fun ExtensionsSubmenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            ExtensionsSubmenu(
                recommendedAddons = listOf(
                    Addon(
                        id = "id",
                        translatableName = mapOf(Addon.DEFAULT_LOCALE to "name"),
                        translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description"),
                        translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary"),
                    ),
                    Addon(
                        id = "id",
                        translatableName = mapOf(Addon.DEFAULT_LOCALE to "name"),
                        translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description"),
                        translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary"),
                    ),
                ),
                showExtensionsOnboarding = true,
                showDisabledExtensionsOnboarding = true,
                showManageExtensions = true,
                addonInstallationInProgress = Addon(
                    id = "id",
                    translatableName = mapOf(Addon.DEFAULT_LOCALE to "name"),
                    translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description"),
                    translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary"),
                ),
                webExtensionMenuItems = listOf(
                    WebExtensionMenuItem(
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
                onBackButtonClick = {},
                onExtensionsLearnMoreClick = {},
                onManageExtensionsMenuClick = {},
                onAddonClick = {},
                onInstallAddonClick = {},
                onDiscoverMoreExtensionsMenuClick = {},
                webExtensionMenuItemClick = {},
            )
        }
    }
}

@Preview
@Composable
private fun ExtensionsSubmenuPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier.background(color = FirefoxTheme.colors.layer3),
        ) {
            ExtensionsSubmenu(
                recommendedAddons = listOf(
                    Addon(
                        id = "id",
                        translatableName = mapOf(Addon.DEFAULT_LOCALE to "name"),
                        translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description"),
                        translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary"),
                    ),
                    Addon(
                        id = "id",
                        translatableName = mapOf(Addon.DEFAULT_LOCALE to "name"),
                        translatableDescription = mapOf(Addon.DEFAULT_LOCALE to "description"),
                        translatableSummary = mapOf(Addon.DEFAULT_LOCALE to "summary"),
                    ),
                ),
                webExtensionMenuItems = listOf(
                    WebExtensionMenuItem(
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
                showExtensionsOnboarding = true,
                showDisabledExtensionsOnboarding = false,
                showManageExtensions = false,
                addonInstallationInProgress = null,
                onBackButtonClick = {},
                onExtensionsLearnMoreClick = {},
                onManageExtensionsMenuClick = {},
                onAddonClick = {},
                onInstallAddonClick = {},
                onDiscoverMoreExtensionsMenuClick = {},
                webExtensionMenuItemClick = {},
            )
        }
    }
}
