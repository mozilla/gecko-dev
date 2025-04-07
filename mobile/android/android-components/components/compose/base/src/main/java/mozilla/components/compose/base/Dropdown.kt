/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base

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
import androidx.compose.runtime.derivedStateOf
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
import mozilla.components.compose.base.annotation.FlexibleWindowLightDarkPreview
import mozilla.components.compose.base.menu.DropdownMenu
import mozilla.components.compose.base.menu.MenuItem
import mozilla.components.compose.base.text.Text
import mozilla.components.compose.base.text.value
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.ui.icons.R as iconsR

private val IconSize = 24.dp
private val HorizontalPadding = 4.dp
private val IconSpace = 12.dp

// The default padding from androidx.compose.material.DropdownMenuItemHorizontalPadding
private val DefaultDropdownMenuItemHorizontalPadding = 16.dp

private val ContextMenuWidth =
    2 * HorizontalPadding +
        IconSize +
        IconSpace +
        2 * DefaultDropdownMenuItemHorizontalPadding

/**
 * UI for a dropdown and a contextual menu that can be expanded or collapsed.
 *
 * @param label Text to be displayed above the dropdown.
 * @param placeholder The text to be displayed when no [dropdownItems] are selected.
 * @param dropdownItems The [MenuItem.CheckableItem]s that should be shown when the dropdown is expanded.
 * @param modifier Modifier to be applied to the dropdown layout.
 * @param dropdownMenuTextWidth The optional width to allocate for the text for each [MenuItem.CheckableItem].
 * If not specified, the best width will be determined based on the dropdown items provided.
 * @param isInLandscapeMode Whether the device is in landscape mode.
 */
@Suppress("LongMethod")
@Composable
fun Dropdown(
    label: String,
    placeholder: String,
    dropdownItems: List<MenuItem.CheckableItem>,
    modifier: Modifier = Modifier,
    dropdownMenuTextWidth: Dp? = null,
    isInLandscapeMode: Boolean = false,
) {
    val dropdownMenuWidth: Dp
    if (dropdownMenuTextWidth != null) {
        dropdownMenuWidth = ContextMenuWidth + dropdownMenuTextWidth
    } else {
        val longestDropdownItemSize = getLongestItemWidth(dropdownItems, AcornTheme.typography.subtitle1)
        dropdownMenuWidth = ContextMenuWidth + longestDropdownItemSize
    }

    val checkedItemText by remember(dropdownItems) {
        derivedStateOf {
            dropdownItems.find { it.isChecked }?.text
        }
    }

    val density = LocalDensity.current

    var expanded by remember { mutableStateOf(false) }

    var measuredDropdownMenuWidthDp by remember { mutableStateOf(0.dp) }

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
            color = AcornTheme.colors.textPrimary,
            style = AcornTheme.typography.caption,
        )

        Spacer(modifier = Modifier.height(4.dp))

        val placeholderText = checkedItemText?.value ?: placeholder

        Box {
            Row {
                Text(
                    text = placeholderText,
                    modifier = Modifier.weight(1f),
                    color = AcornTheme.colors.textPrimary,
                    style = AcornTheme.typography.subtitle1,
                )

                Spacer(modifier = Modifier.width(10.dp))

                Icon(
                    painter = painterResource(id = iconsR.drawable.mozac_ic_dropdown_arrow),
                    contentDescription = null,
                    tint = AcornTheme.colors.iconPrimary,
                )
            }

            Box(modifier = Modifier.align(Alignment.TopEnd)) {
                DropdownMenu(
                    menuItems = dropdownItems,
                    expanded = expanded,
                    modifier = Modifier
                        .onGloballyPositioned { coordinates ->
                            measuredDropdownMenuWidthDp = with(density) {
                                coordinates.size.width.toDp()
                            }
                        }
                        .requiredSizeIn(
                            maxHeight = 200.dp,
                            maxWidth = dropdownMenuWidth,
                            minWidth = dropdownMenuWidth,
                        ),
                    offset = if (isInLandscapeMode) {
                        DpOffset(
                            -measuredDropdownMenuWidthDp,
                            IconSize,
                        )
                    } else {
                        DpOffset(
                            0.dp,
                            IconSize,
                        )
                    },
                    onDismissRequest = { expanded = false },
                )
            }
        }

        Divider(color = AcornTheme.colors.formDefault)
    }
}

@Composable
private fun getLongestItemWidth(items: List<MenuItem.CheckableItem>, style: TextStyle): Dp {
    if (items.isEmpty()) {
        return 0.dp
    }

    val textMeasurer = rememberTextMeasurer(cacheSize = items.size)
    val longestDropdownItemSize = items.maxOf {
        val width = textMeasurer.measure(
            text = it.text.value,
            style = style,
        ).size.width

        with(LocalDensity.current) { width.toDp() }
    }
    return longestDropdownItemSize
}

@Suppress("MagicNumber")
private fun getDropdownItems(): List<MenuItem.CheckableItem> =
    List(10) { index ->
        MenuItem.CheckableItem(
            text = Text.String("Item $index"),
            isChecked = false,
            onClick = {},
        )
    }

private fun getSelectedDropdownItems(): List<MenuItem.CheckableItem> =
    listOf(
        MenuItem.CheckableItem(
            text = Text.String("Item 1"),
            isChecked = true,
            onClick = {},
        ),
        MenuItem.CheckableItem(
            text = Text.String("Item 2"),
            isChecked = false,
            onClick = {},
        ),
        MenuItem.CheckableItem(
            text = Text.String("Item 3"),
            isChecked = false,
            onClick = {},
        ),
    )

@FlexibleWindowLightDarkPreview
@Composable
private fun DropdownPreview() {
    AcornTheme {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(color = AcornTheme.colors.layer1),
        ) {
            Dropdown(
                label = "Placeholder and nothing selected",
                dropdownItems = getDropdownItems(),
                placeholder = "Placeholder",
                modifier = Modifier.fillMaxWidth(),
            )

            Spacer(modifier = Modifier.height(AcornTheme.layout.space.dynamic150))

            Dropdown(
                label = "Placeholder and item selected",
                placeholder = "Placeholder",
                dropdownItems = getSelectedDropdownItems(),
                modifier = Modifier.fillMaxWidth(),
            )
        }
    }
}
