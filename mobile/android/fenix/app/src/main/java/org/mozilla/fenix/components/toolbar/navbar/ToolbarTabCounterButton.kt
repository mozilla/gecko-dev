/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import android.content.res.Configuration
import android.view.View
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.material.minimumInteractiveComponentSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import mozilla.components.compose.base.annotation.LightDarkPreview
import mozilla.components.ui.tabcounter.TabCounter
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

// Interim composable for a tab counter button that supports showing a menu on long press.
// With this being implemented as an AndroidView the menu can be shown as low to the bottom of the screen as needed.
// To be replaced with a fully Compose implementation in the future that use a DropdownMenu once
// https://github.com/JetBrains/compose-multiplatform/issues/1878 is resolved.

/**
 * Composable that delegates to an AndroidView to display a tab counter button and optionally a menu.
 * If a menu is provided it will be shown as low to the bottom of the screen as needed and will be shown
 * on long presses of the tab counter button irrespective of the [onLongPress] callback being set or not.
 *
 * @param tabCount The number of tabs to display in the tab counter.
 * @param isPrivateMode Whether the browser is in private mode.
 * @param onClick Invoked when the tab counter is clicked.
 * @param menu Optional lazy menu to show when the tab counter is long clicked.
 * @param onLongPress Optional callback for when the tab counter is long clicked.
 */
@Composable
fun ToolbarTabCounterButton(
    tabCount: Int,
    isPrivateMode: Boolean,
    onClick: () -> Unit,
    menu: Lazy<TabCounterMenu>? = null,
    onLongPress: () -> Unit = {},
) {
    val isRtl = LocalLayoutDirection.current == LayoutDirection.Rtl
    AndroidView(
        factory = { context ->
            TabCounter(context).apply {
                setOnClickListener {
                    onClick()
                }

                setOnLongClickListener {
                    menu?.value?.let { menu ->
                        onLongPress()
                        menu.menuController.show(anchor = it)
                        true
                    } ?: false
                }

                contentDescription = context.getString(
                    R.string.mozac_tab_counter_open_tab_tray,
                    tabCount.toString(),
                )
                toggleCounterMask(isPrivateMode)
                setBackgroundResource(R.drawable.mozac_material_ripple_minimum_interaction_size)
            }
        },
        modifier = Modifier
            .minimumInteractiveComponentSize()
            .testTag(NavBarTestTags.tabCounterButton),
        update = { tabCounter ->
            tabCounter.setCount(tabCount)
            tabCounter.layoutDirection = if (isRtl) {
                View.TEXT_DIRECTION_RTL
            } else {
                View.TEXT_DIRECTION_LTR
            }
        },
    )
}

@Suppress("MagicNumber")
@LightDarkPreview
@Composable
private fun ToolbarTabCounterButtonPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(10.dp),
        ) {
            ToolbarTabCounterButton(
                tabCount = 5,
                isPrivateMode = false,
                onClick = {},
            )
        }
    }
}

@Suppress("MagicNumber")
@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun ToolbarTabCounterButtonWithFeltPrivacyPreview() {
    FirefoxTheme(theme = Theme.Private) {
        Box(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(10.dp),
        ) {
            ToolbarTabCounterButton(
                tabCount = 5,
                isPrivateMode = true,
                onClick = {},
            )
        }
    }
}
