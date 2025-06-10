/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.compose.header

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.LocalIndication
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.PlatformTextStyle
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import org.mozilla.fenix.R
import org.mozilla.fenix.components.menu.compose.MenuItemState
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

@Suppress("LongParameterList")
@Composable
internal fun MenuNavHeader(
    state: MenuItemState = MenuItemState.ENABLED,
    isSiteLoading: Boolean,
    onBackButtonClick: (longPress: Boolean) -> Unit,
    onForwardButtonClick: (longPress: Boolean) -> Unit,
    onRefreshButtonClick: (longPress: Boolean) -> Unit,
    onStopButtonClick: () -> Unit,
    onShareButtonClick: () -> Unit,
    isExtensionsExpanded: Boolean,
    isMoreMenuExpanded: Boolean,
) {
    Spacer(
        modifier = Modifier
            .fillMaxWidth()
            .height(12.dp)
            .background(
                if (isExtensionsExpanded || isMoreMenuExpanded) {
                    FirefoxTheme.colors.layerSearch
                } else {
                    Color.Transparent
                },
            ),
    )

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(
                color = if (isExtensionsExpanded || isMoreMenuExpanded) {
                    FirefoxTheme.colors.layerSearch
                } else {
                    Color.Transparent
                },
            )
            .padding(horizontal = 16.dp, vertical = 12.dp)
            .verticalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        MenuNavItem(
            state = state,
            painter = painterResource(id = R.drawable.mozac_ic_back_24),
            label = stringResource(id = R.string.browser_menu_back),
            onClick = { onBackButtonClick(false) },
            onLongClick = { onBackButtonClick(true) },
        )

        MenuNavItem(
            state = state,
            painter = painterResource(id = R.drawable.mozac_ic_forward_24),
            label = stringResource(id = R.string.browser_menu_forward),
            onClick = { onForwardButtonClick(false) },
            onLongClick = { onForwardButtonClick(true) },
        )

        if (isSiteLoading) {
            MenuNavItem(
                state = state,
                painter = painterResource(id = R.drawable.mozac_ic_stop),
                label = stringResource(id = R.string.browser_menu_stop),
                onClick = onStopButtonClick,
            )
        } else {
            MenuNavItem(
                state = state,
                painter = painterResource(id = R.drawable.mozac_ic_arrow_clockwise_24),
                label = stringResource(id = R.string.browser_menu_refresh),
                onClick = { onRefreshButtonClick(false) },
                onLongClick = { onRefreshButtonClick(true) },
            )
        }

        MenuNavItem(
            state = state,
            painter = painterResource(id = R.drawable.mozac_ic_share_android_24),
            label = stringResource(id = R.string.browser_menu_share),
            onClick = onShareButtonClick,
        )
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun MenuNavItem(
    state: MenuItemState = MenuItemState.ENABLED,
    painter: Painter,
    label: String,
    onClick: () -> Unit = {},
    onLongClick: (() -> Unit)? = null,
) {
    Column(
        modifier = Modifier
            .width(64.dp)
            .height(48.dp)
            .combinedClickable(
                interactionSource = null,
                indication = LocalIndication.current,
                enabled = state != MenuItemState.DISABLED,
                onClick = onClick,
                onLongClick = onLongClick,
            ),
        verticalArrangement = Arrangement.spacedBy(4.dp, Alignment.CenterVertically),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Icon(
            painter = painter,
            contentDescription = null,
            tint = getIconTint(state = state),
        )

        Text(
            text = label,
            color = getLabelTextColor(state = state),
            maxLines = 2,
            style = FirefoxTheme.typography.caption.merge(
                platformStyle = PlatformTextStyle(includeFontPadding = true),
            ),
        )
    }
}

@Composable
private fun getLabelTextColor(state: MenuItemState): Color {
    return when (state) {
        MenuItemState.ACTIVE -> FirefoxTheme.colors.textAccent
        MenuItemState.WARNING -> FirefoxTheme.colors.textCritical
        MenuItemState.DISABLED -> FirefoxTheme.colors.textDisabled
        else -> FirefoxTheme.colors.textPrimary
    }
}

@Composable
private fun getIconTint(state: MenuItemState): Color {
    return when (state) {
        MenuItemState.ACTIVE -> FirefoxTheme.colors.iconAccentViolet
        MenuItemState.WARNING -> FirefoxTheme.colors.iconCritical
        MenuItemState.DISABLED -> FirefoxTheme.colors.iconDisabled
        else -> FirefoxTheme.colors.iconSecondary
    }
}

@PreviewLightDark
@Composable
private fun MenuHeaderPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            MenuNavHeader(
                isSiteLoading = false,
                onBackButtonClick = {},
                onForwardButtonClick = {},
                onRefreshButtonClick = {},
                onStopButtonClick = {},
                onShareButtonClick = {},
                isExtensionsExpanded = false,
                isMoreMenuExpanded = false,
            )
        }
    }
}

@Preview
@Composable
private fun MenuHeaderPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer3),
        ) {
            MenuNavHeader(
                isSiteLoading = false,
                onBackButtonClick = {},
                onForwardButtonClick = {},
                onRefreshButtonClick = {},
                onStopButtonClick = {},
                onShareButtonClick = {},
                isExtensionsExpanded = false,
                isMoreMenuExpanded = false,
            )
        }
    }
}
