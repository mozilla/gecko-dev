/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import android.view.SoundEffectConstants
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.material3.minimumInteractiveComponentSize
import androidx.compose.material3.ripple
import androidx.compose.runtime.Composable
import androidx.compose.runtime.Stable
import androidx.compose.runtime.key
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import com.google.accompanist.drawablepainter.rememberDrawablePainter
import mozilla.components.compose.base.Divider
import mozilla.components.compose.base.modifier.thenConditional
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.CombinedEventAndMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuButton
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem.BrowserToolbarMenuDivider

@Stable
internal fun BrowserToolbarInteraction.toMenuItems(): List<BrowserToolbarMenuItem> = when (this) {
    is BrowserToolbarMenu -> items()
    is CombinedEventAndMenu -> menu.items()
    else -> emptyList()
}

@Composable
internal fun menuItemComposable(
    source: BrowserToolbarMenuItem,
    onInteraction: (BrowserToolbarEvent) -> Unit,
): @Composable () -> Unit {
    return when (source) {
        is BrowserToolbarMenuButton -> {
            @Composable {
                if (source.text != null) {
                    val view = LocalView.current
                    val contentDescription = stringResource(source.text)
                    val iconPainter = key(source) { source.iconPainter() }

                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier
                            .thenConditional(
                                Modifier.clickable(
                                    role = Role.Button,
                                    interactionSource = remember { MutableInteractionSource() },
                                    indication = ripple(
                                        bounded = true,
                                        color = AcornTheme.colors.ripple,
                                    ),
                                    onClick = {
                                        view.playSoundEffect(SoundEffectConstants.CLICK)
                                        source.onClick?.let { onInteraction(it) }
                                    },
                                ),
                            ) { source.onClick != null }
                            .semantics { this.contentDescription = contentDescription }
                            .fillMaxWidth()
                            .minimumInteractiveComponentSize()
                            .padding(horizontal = 16.dp),
                    ) {
                        if (iconPainter != null) {
                            Icon(
                                painter = iconPainter,
                                contentDescription = stringResource(source.text),
                                tint = AcornTheme.colors.iconPrimary,
                            )
                            Spacer(modifier = Modifier.width(20.dp))
                        }

                        Text(
                            text = stringResource(source.text),
                            modifier = Modifier
                                .fillMaxSize()
                                .wrapContentSize(Alignment.CenterStart),
                            color = AcornTheme.colors.textPrimary,
                            maxLines = 1,
                            style = AcornTheme.typography.subtitle1,
                        )
                    }
                }
            }
        }

        is BrowserToolbarMenuDivider -> {
            @Composable {
                Divider(
                    color = AcornTheme.colors.borderSecondary,
                )
            }
        }
    }
}

@Composable
private fun BrowserToolbarMenuButton.iconPainter() = when {
    icon != null -> rememberDrawablePainter(icon)
    iconResource != null -> painterResource(iconResource)
    else -> null
}
