/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.compose.menu

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Icon
import androidx.compose.material.MaterialTheme
import androidx.compose.material.MenuDefaults
import androidx.compose.material.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.compositionLocalOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.traversalIndex
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.DpOffset
import androidx.compose.ui.unit.dp
import mozilla.components.compose.base.Divider
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.button.PrimaryButton
import org.mozilla.fenix.compose.ext.thenConditional
import org.mozilla.fenix.compose.menu.MenuItem.FixedItem.Level
import org.mozilla.fenix.compose.text.Text
import org.mozilla.fenix.compose.text.value
import org.mozilla.fenix.theme.FirefoxTheme
import androidx.compose.material.DropdownMenu as MaterialDropdownMenu
import androidx.compose.material.DropdownMenuItem as MaterialDropdownMenuItem

private val MenuItemHeight = 48.dp
private val ItemHorizontalSpaceBetween = 16.dp
private val defaultMenuShape = RoundedCornerShape(8.dp)

/**
 * A dropdown menu that displays a list of [MenuItem]s. The menu can be expanded or collapsed and
 * is displayed as a popup anchored to the menu button that triggers it.
 *
 * @param menuItems the list of [MenuItem]s to display in the menu.
 * @param expanded whether or not the menu is expanded.
 * @param modifier [Modifier] to be applied to the menu.
 * @param offset [DpOffset] from the original anchor position of the menu.
 * @param scrollState [ScrollState] used by the menu's content for vertical scrolling.
 * @param onDismissRequest Invoked when the user requests to dismiss the menu, such as by tapping
 * outside the menu's bounds.
 */
@Composable
fun DropdownMenu(
    menuItems: List<MenuItem>,
    expanded: Boolean,
    modifier: Modifier = Modifier,
    offset: DpOffset = DpOffset(0.dp, 0.dp),
    scrollState: ScrollState = rememberScrollState(),
    onDismissRequest: () -> Unit,
) {
    MaterialTheme(shapes = MaterialTheme.shapes.copy(medium = defaultMenuShape)) {
        MaterialDropdownMenu(
            expanded = expanded,
            onDismissRequest = onDismissRequest,
            offset = offset,
            scrollState = scrollState,
            modifier = modifier.background(FirefoxTheme.colors.layer2),
        ) {
            DropdownMenuContent(
                menuItems = menuItems,
                onDismissRequest = onDismissRequest,
            )

            val density = LocalDensity.current

            LaunchedEffect(Unit) {
                if (expanded) {
                    menuItems.indexOfFirst {
                        it is MenuItem.CheckableItem && it.isChecked
                    }.takeIf { it != -1 }?.let { index ->
                        val scrollPosition = with(density) { MenuItemHeight.toPx() * index }.toInt()
                        scrollState.scrollTo(scrollPosition)
                    }
                }
            }
        }
    }
}

@Composable
private fun DropdownMenuContent(
    menuItems: List<MenuItem>,
    onDismissRequest: () -> Unit,
) {
    menuItems.forEach {
        when (it) {
            is MenuItem.FixedItem -> {
                CompositionLocalProvider(LocalLevelColor provides it.level) {
                    when (it) {
                        is MenuItem.TextItem -> FlexibleDropdownMenuItem(
                            onClick = {
                                onDismissRequest()
                                it.onClick()
                            },
                            modifier = Modifier
                                .testTag(it.testTag),
                            content = {
                                TextMenuItemContent(item = it)
                            },
                        )

                        is MenuItem.IconItem -> FlexibleDropdownMenuItem(
                            onClick = {
                                onDismissRequest()
                                it.onClick()
                            },
                            modifier = Modifier
                                .testTag(it.testTag),
                            content = {
                                IconMenuItemContent(item = it)
                            },
                        )

                        is MenuItem.CheckableItem -> FlexibleDropdownMenuItem(
                            modifier = Modifier
                                .selectable(
                                    selected = it.isChecked,
                                    role = Role.Button,
                                    onClick = {
                                        onDismissRequest()
                                        it.onClick()
                                    },
                                )
                                .testTag(it.testTag)
                                .thenConditional(
                                    modifier = Modifier.semantics { traversalIndex = -1f },
                                ) { it.isChecked },
                            onClick = {
                                onDismissRequest()
                                it.onClick()
                            },
                            content = {
                                CheckableMenuItemContent(item = it)
                            },
                        )
                    }
                }
            }

            is MenuItem.CustomMenuItem -> FlexibleDropdownMenuItem(
                onClick = {},
                content = {
                    it.content()
                },
            )

            is MenuItem.Divider -> Divider()
        }
    }
}

@Composable
private fun TextMenuItemContent(
    item: MenuItem.TextItem,
) {
    MenuItemText(item.text)
}

@Composable
private fun CheckableMenuItemContent(
    item: MenuItem.CheckableItem,
) {
    if (item.isChecked) {
        Icon(
            painter = painterResource(R.drawable.mozac_ic_checkmark_24),
            tint = FirefoxTheme.levelColors.iconPrimary,
            contentDescription = null,
        )
    } else {
        Spacer(modifier = Modifier.size(24.dp))
    }

    MenuItemText(item.text)
}

@Composable
private fun IconMenuItemContent(
    item: MenuItem.IconItem,
) {
    Icon(
        painter = painterResource(item.drawableRes),
        tint = FirefoxTheme.levelColors.iconPrimary,
        contentDescription = null,
    )

    MenuItemText(item.text)
}

@Composable
private fun FlexibleDropdownMenuItem(
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    contentPadding: PaddingValues = MenuDefaults.DropdownMenuItemContentPadding,
    interactionSource: MutableInteractionSource? = null,
    content: @Composable () -> Unit,
) {
    MaterialDropdownMenuItem(
        onClick = onClick,
        modifier = modifier
            .height(MenuItemHeight)
            .semantics(mergeDescendants = true) {},
        enabled = enabled,
        contentPadding = contentPadding,
        interactionSource = interactionSource,
        content = {
            Row(
                horizontalArrangement = Arrangement.spacedBy(ItemHorizontalSpaceBetween),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                content()
            }
        },
    )
}

@Composable
private fun MenuItemText(text: Text) {
    Text(
        text = text.value,
        color = FirefoxTheme.levelColors.textPrimary,
        style = FirefoxTheme.typography.subtitle1,
    )
}

/**
 * A subset of the color palette that is used to style a UI element based on their level.
 *
 * @property textPrimary The primary text color.
 * @property iconPrimary The primary icon color.
 */
private data class LevelColors(
    val textPrimary: Color,
    val iconPrimary: Color,
)

/**
 * CompositionLocal that provides the current level of importance.
 */
private val LocalLevelColor = compositionLocalOf { Level.Default }

/**
 * Returns the [LevelColors] based on the current level of importance.
 */
private val FirefoxTheme.levelColors: LevelColors
    @Composable
    get() {
        val current = LocalLevelColor.current
        return when (current) {
            Level.Default -> {
                LevelColors(
                    textPrimary = colors.textPrimary,
                    iconPrimary = colors.iconPrimary,
                )
            }

            Level.Critical -> {
                LevelColors(
                    textPrimary = colors.textCritical,
                    iconPrimary = colors.iconCritical,
                )
            }
        }
    }

private data class MenuPreviewParameter(
    val itemType: ItemType,
    val menuItems: List<MenuItem>,
) {
    enum class ItemType {
        TEXT_ITEMS,
        CHECKABLE_ITEMS,
        ICON_ITEMS,
    }
}

private val menuPreviewParameters by lazy {
    listOf(
        MenuPreviewParameter(
            itemType = MenuPreviewParameter.ItemType.TEXT_ITEMS,
            menuItems = listOf(
                MenuItem.TextItem(
                    text = Text.String("Text Item 1"),
                    onClick = {},
                ),
                MenuItem.TextItem(
                    text = Text.String("Text Item 1"),
                    onClick = {},
                ),
            ),
        ),
        MenuPreviewParameter(
            itemType = MenuPreviewParameter.ItemType.CHECKABLE_ITEMS,
            menuItems = listOf(
                MenuItem.CheckableItem(
                    text = Text.String("Checkable Item 1"),
                    isChecked = true,
                    onClick = {},
                ),
                MenuItem.CheckableItem(
                    text = Text.String("Checkable Item 2"),
                    isChecked = false,
                    onClick = {},
                ),
            ),
        ),
        MenuPreviewParameter(
            itemType = MenuPreviewParameter.ItemType.ICON_ITEMS,
            menuItems = listOf(
                MenuItem.IconItem(
                    text = Text.String("Delete"),
                    drawableRes = R.drawable.mozac_ic_delete_24,
                    level = Level.Critical,
                    onClick = {},
                ),
                MenuItem.IconItem(
                    text = Text.String("Have a cookie!"),
                    drawableRes = R.drawable.mozac_ic_cookies_24,
                    onClick = {},
                ),
                MenuItem.Divider,
                MenuItem.IconItem(
                    text = Text.String("What's new"),
                    drawableRes = R.drawable.mozac_ic_whats_new_24,
                    onClick = {},
                ),
            ),
        ),
    )
}

@PreviewLightDark
@Composable
private fun DropdownMenuPreview() {
    FirefoxTheme {
        Column(
            modifier = Modifier
                .background(color = FirefoxTheme.colors.layer1)
                .fillMaxSize()
                .padding(FirefoxTheme.space.baseContentEqualPadding),
            verticalArrangement = Arrangement.spacedBy(FirefoxTheme.space.baseContentEqualPadding),
        ) {
            Text(
                text = "Click buttons to expand dropdown menu",
                style = FirefoxTheme.typography.body1,
                color = FirefoxTheme.colors.textPrimary,
            )

            Text(
                text = """
                    The menu items along with checkable state should be hoisted in feature logic and simply passed to the DropdownMenu composable. The mapping is done here in the composable as an example, try to do that outside the composables.
                """.trimIndent(),
                style = FirefoxTheme.typography.caption,
                color = FirefoxTheme.colors.textPrimary,
            )

            menuPreviewParameters.forEach {
                Box {
                    var expanded by remember { mutableStateOf(false) }
                    val text by remember { mutableStateOf(it.itemType.name.replace("_", " ")) }

                    PrimaryButton(text = text) {
                        expanded = true
                    }
                    DropdownMenu(
                        menuItems = it.menuItems,
                        expanded = expanded,
                        onDismissRequest = { expanded = false },
                    )
                }
            }

            Spacer(modifier = Modifier.size(FirefoxTheme.space.baseContentEqualPadding))

            Text(
                text = "Dropdown menu items",
                style = FirefoxTheme.typography.body1,
                color = FirefoxTheme.colors.textPrimary,
            )

            Column(
                modifier = Modifier.background(color = FirefoxTheme.colors.layer2),
            ) {
                val menuItems: List<MenuItem> by remember {
                    mutableStateOf(menuPreviewParameters.map { it.menuItems.first() })
                }

                DropdownMenuContent(menuItems) { }
            }

            Spacer(modifier = Modifier.size(FirefoxTheme.space.baseContentEqualPadding))

            Text(
                text = "Checkable menu item usage",
                style = FirefoxTheme.typography.body1,
                color = FirefoxTheme.colors.textPrimary,
            )

            Column(
                modifier = Modifier.background(color = FirefoxTheme.colors.layer2),
            ) {
                var isChecked by remember { mutableStateOf(true) }

                DropdownMenuContent(
                    menuItems = listOf(
                        MenuItem.CheckableItem(
                            text = Text.String(value = "Click me!"),
                            isChecked = isChecked,
                            onClick = { isChecked = !isChecked },
                        ),
                    ),
                    onDismissRequest = {},
                )
            }
        }
    }
}
