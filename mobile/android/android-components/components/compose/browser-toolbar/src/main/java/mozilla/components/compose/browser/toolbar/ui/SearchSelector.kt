/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import android.graphics.drawable.Drawable
import android.view.SoundEffectConstants
import androidx.annotation.DrawableRes
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import com.google.accompanist.drawablepainter.rememberDrawablePainter
import mozilla.components.compose.base.menu.CustomPlacementPopup
import mozilla.components.compose.base.menu.CustomPlacementPopupVerticalContent
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.R
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarEvent
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarInteraction.BrowserToolbarMenu
import mozilla.components.compose.browser.toolbar.store.BrowserToolbarMenuItem
import mozilla.components.ui.icons.R as iconsR

/**
 * Search selector toolbar action.
 *
 * @property icon A [Drawable] to display in the search selector. If null will use [fallbackIcon] instead.
 * @param contentDescription The content description to use.
 * @param menu The [BrowserToolbarMenu] to show when the search selector is clicked.
 * @param onInteraction Invoked for user interactions with the menu items.
 * @param fallbackIcon The resource id of the icon to use for this button if a [Drawable] is not provided.
 */
@Composable
fun SearchSelector(
    icon: Drawable?,
    contentDescription: String,
    menu: BrowserToolbarMenu,
    onInteraction: (BrowserToolbarEvent) -> Unit,
    @DrawableRes fallbackIcon: Int = iconsR.drawable.mozac_ic_search_24,
) {
    val view = LocalView.current
    var showMenu by remember { mutableStateOf(false) }

    Card(
        modifier = Modifier
            .padding(horizontal = 8.dp)
            .semantics(mergeDescendants = true) {
                this.contentDescription = contentDescription
            }
            .clickable {
                view.playSoundEffect(SoundEffectConstants.CLICK)
                showMenu = true
            },
        shape = RoundedCornerShape(4.dp),
        colors = CardDefaults.cardColors(
            containerColor = AcornTheme.colors.layer2,
        ),
        elevation = CardDefaults.elevatedCardElevation(
            defaultElevation = 0.dp,
        ),
    ) {
        Row(
            modifier = Modifier.padding(
                start = 2.dp,
                top = 2.dp,
                end = 4.dp,
                bottom = 2.dp,
            ),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Icon(
                painter = when (icon) {
                    null -> painterResource(fallbackIcon)
                    else -> rememberDrawablePainter(drawable = icon)
                },
                contentDescription = null,
                modifier = Modifier
                    .size(24.dp)
                    .clip(RoundedCornerShape(2.dp)),
                tint = AcornTheme.colors.iconPrimary,
            )

            Spacer(modifier = Modifier.width(4.dp))

            Icon(
                painter = painterResource(R.drawable.mozac_compose_browser_toolbar_chevron_down_6),
                contentDescription = null,
                tint = AcornTheme.colors.iconPrimary,
            )
        }

        CustomPlacementPopup(
            isVisible = showMenu,
            onDismissRequest = { showMenu = false },
        ) {
            CustomPlacementPopupVerticalContent {
                menu.toMenuItems().forEach { menuItem ->
                    menuItemComposable(menuItem) { event ->
                        showMenu = false
                        onInteraction(event)
                    }.invoke()
                }
            }
        }
    }
}

@PreviewLightDark
@Composable
private fun SearchSelectorPreview() {
    AcornTheme {
        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
            SearchSelector(
                icon = null,
                contentDescription = "test",
                menu = object : BrowserToolbarMenu {
                    override fun items() = emptyList<BrowserToolbarMenuItem>()
                },
                onInteraction = {},
            )
        }
    }
}
