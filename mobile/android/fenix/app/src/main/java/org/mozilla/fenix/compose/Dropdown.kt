/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.requiredSizeIn
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.wrapContentHeight
import androidx.compose.foundation.layout.wrapContentSize
import androidx.compose.material.Icon
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.rememberTextMeasurer
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.times
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.annotation.FlexibleWindowLightDarkPreview
import org.mozilla.fenix.theme.FirefoxTheme

private val ICON_SIZE = 24.dp

/**
 * UI for a dropdown and a contextual menu that can be expanded or collapsed.
 *
 * @param label Text to be displayed above the dropdown.
 * @param placeholder The text to be displayed when no [dropdownItems] are selected.
 * @param dropdownItems The [MenuItem]s that should be shown when the dropdown is expanded.
 * @param modifier Modifier to be applied to the dropdown layout.
 * @param dropdownMenuTextWidth The optional width to allocate for the text for each [MenuItem]. If
 * not specified, the best width will be determined based on the dropdown items provided.
 * @param isInLandscapeMode Whether the device is in landscape mode.
 */
@Suppress("LongMethod", "Deprecation") // https://bugzilla.mozilla.org/show_bug.cgi?id=1927716
@Composable
fun Dropdown(
    label: String,
    placeholder: String,
    dropdownItems: List<MenuItem>,
    modifier: Modifier = Modifier,
    dropdownMenuTextWidth: Dp? = null,
    isInLandscapeMode: Boolean = false,
) {
    val horizontalPadding = 4.dp
    // The default padding from androidx.compose.material.DropdownMenuItemHorizontalPadding
    val defaultDropdownMenuItemHorizontalPadding = 16.dp
    val iconSpace = 12.dp

    var contextMenuWidth =
        2 * horizontalPadding +
            ICON_SIZE +
            iconSpace +
            2 * defaultDropdownMenuItemHorizontalPadding

    if (dropdownMenuTextWidth != null) {
        contextMenuWidth += dropdownMenuTextWidth
    } else {
        val longestDropdownItemSize = getLongestItemWidth(dropdownItems, FirefoxTheme.typography.subtitle1)
        contextMenuWidth += longestDropdownItemSize
    }

    val density = LocalDensity.current

    var expanded by remember { mutableStateOf(false) }

    var contextMenuWidthDp by remember { mutableStateOf(0.dp) }

    Column(
        modifier = modifier
            .clickable {
                expanded = true
            }
            .semantics { role = Role.DropdownList },
    ) {
        Text(
            text = label,
            modifier = Modifier
                .wrapContentSize()
                .defaultMinSize(minHeight = 16.dp)
                .wrapContentHeight(),
            color = FirefoxTheme.colors.textPrimary,
            style = FirefoxTheme.typography.caption,
        )

        Spacer(modifier = Modifier.height(4.dp))

        val placeholderText = dropdownItems.find { it.isChecked }?.title ?: placeholder

        Box {
            Row {
                Text(
                    text = placeholderText,
                    modifier = Modifier.weight(1f),
                    color = FirefoxTheme.colors.textPrimary,
                    style = FirefoxTheme.typography.subtitle1,
                )

                Spacer(modifier = Modifier.width(10.dp))

                Icon(
                    painter = painterResource(id = R.drawable.mozac_ic_dropdown_arrow),
                    contentDescription = null,
                    tint = FirefoxTheme.colors.iconPrimary,
                )
            }

            if (expanded) {
                Box(modifier = Modifier.align(Alignment.TopEnd)) {
                    ContextualMenu(
                        showMenu = true,
                        onDismissRequest = {
                            expanded = false
                        },
                        menuItems = dropdownItems,
                        modifier = Modifier
                            .onGloballyPositioned { coordinates ->
                                contextMenuWidthDp = with(density) {
                                    coordinates.size.width.toDp()
                                }
                            }
                            .requiredSizeIn(
                                maxHeight = 200.dp,
                                maxWidth = contextMenuWidth,
                                minWidth = contextMenuWidth,
                            ),
                        canShowCheckItems = true,
                        offset = if (isInLandscapeMode) {
                            DpOffset(
                                -contextMenuWidthDp,
                                ICON_SIZE,
                            )
                        } else {
                            DpOffset(
                                0.dp,
                                ICON_SIZE,
                            )
                        },
                    )
                }
            }
        }

        Divider(color = FirefoxTheme.colors.formDefault)
    }
}

@Composable
private fun getLongestItemWidth(items: List<MenuItem>, style: TextStyle): Dp {
    if (items.isEmpty()) {
        return 0.dp
    }

    val textMeasurer = rememberTextMeasurer(cacheSize = items.size)
    val longestDropdownItemSize = items.maxOf {
        val width = textMeasurer.measure(
            text = it.title,
            style = style,
        ).size.width

        with(LocalDensity.current) { width.toDp() }
    }
    return longestDropdownItemSize
}

@Suppress("MagicNumber")
private fun getDropdownItems(): List<MenuItem> =
    List(10) { index ->
        MenuItem(
            title = "Item $index",
            onClick = {},
        )
    }

private fun getSelectedDropdownItems(): List<MenuItem> =
    listOf(
        MenuItem(
            title = "Item 1",
            isChecked = true,
            onClick = {},
        ),
        MenuItem(
            title = "Item 2",
            onClick = {},
        ),
        MenuItem(
            title = "Item 3",
            onClick = {},
        ),
    )

@FlexibleWindowLightDarkPreview
@Composable
private fun DropdownPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(color = FirefoxTheme.colors.layer1),
        ) {
            Dropdown(
                label = "Placeholder and nothing selected",
                dropdownItems = getDropdownItems(),
                placeholder = "Placeholder",
                modifier = Modifier.fillMaxWidth(),
            )

            Spacer(modifier = Modifier.height(FirefoxTheme.space.xSmall))

            Dropdown(
                label = "Placeholder and item selected",
                placeholder = "Placeholder",
                dropdownItems = getSelectedDropdownItems(),
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
