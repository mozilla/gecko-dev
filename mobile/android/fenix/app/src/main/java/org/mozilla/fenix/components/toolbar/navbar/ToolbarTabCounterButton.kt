/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import android.content.res.Configuration
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material.IconButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import mozilla.components.feature.tabs.R
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.ui.tabcounter.TabCounter
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.compose.annotation.LightDarkPreview
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
    IconButton(
        onClick = onClick, // This ensures the 48dp touch target for clicks.
    ) {
        AndroidView(
            factory = { context ->
                TabCounter(context).apply {
                    setOnClickListener {
                        onClick() // This ensures clicks in the 34dp touch target are caught.
                    }

                    setOnLongClickListener {
                        menu?.value?.let { menu ->
                            onLongPress()
                            menu.menuController.show(anchor = it)
                            true
                        } ?: false
                    }

                    contentDescription = context.getString(R.string.mozac_feature_tabs_toolbar_tabs_button)

                    toggleCounterMask(isPrivateMode)
                    setBackgroundResource(
                        context.theme.resolveAttribute(
                            android.R.attr.selectableItemBackgroundBorderless,
                        ),
                    )
                }
            },
            // The IconButton composable has a 48dp size and it's own ripple with a 24dp radius.
            // The TabCounter view has it's own inherent ripple that has a bigger radius
            // so based on manual testing we set a size of 34dp for this View which would
            // ensure it's ripple matches the composable one. Otherwise there is a visible mismatch.
            modifier = Modifier.size(34.dp),
            update = { tabCounter ->
                tabCounter.setCount(tabCount)
            },
        )
    }
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
