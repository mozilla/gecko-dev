/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.browser.toolbar.ui

import android.content.Context
import android.graphics.drawable.Drawable
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.RelativeLayout
import androidx.appcompat.content.res.AppCompatResources.getDrawable
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.Card
import androidx.compose.material.Icon
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.tooling.preview.PreviewLightDark
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import mozilla.components.compose.base.theme.AcornTheme
import mozilla.components.compose.browser.toolbar.R
import mozilla.components.compose.browser.toolbar.databinding.SearchSelectorBinding
import mozilla.components.concept.menu.MenuController
import mozilla.components.ui.icons.R as iconsR

/**
 * Search selector toolbar action.
 *
 * @param painter The [Painter] to draw.
 * @param tint Tint applied to the icon.
 * @param onClick Invoked when the search selector is clicked.
 */
@Composable
fun SearchSelector(
    painter: Painter,
    tint: Color,
    onClick: () -> Unit,
) {
    Card(
        modifier = Modifier
            .padding(horizontal = 8.dp)
            .semantics(mergeDescendants = true) {}
            .clickable(onClick = onClick),
        shape = RoundedCornerShape(4.dp),
        backgroundColor = AcornTheme.colors.layer2,
        elevation = 0.dp,
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
            Image(
                painter = painter,
                contentDescription = null,
                modifier = Modifier
                    .size(24.dp)
                    .clip(RoundedCornerShape(2.dp)),
                contentScale = ContentScale.Crop,
                colorFilter = ColorFilter.tint(tint),
            )

            Spacer(modifier = Modifier.width(4.dp))

            Icon(
                painter = painterResource(R.drawable.mozac_compose_browser_toolbar_chevron_down_6),
                contentDescription = null,
                tint = tint,
            )
        }
    }
}

/**
 * Search selector toolbar action.
 *
 * @param onClick Invoked when the search selector is clicked.
 * @param menu [MenuController] that will be used to create a menu when the search selector is
 * clicked.
 * @param icon [Drawable] to display in the search selector.
 * @param contentDescription The content description to use.
 */
@Composable
fun SearchSelector(
    onClick: () -> Unit,
    menu: MenuController? = null,
    icon: Drawable? = null,
    contentDescription: String? = null,
) {
    AndroidView(
        factory = { context ->
            SearchSelector(context).apply {
                setOnClickListener {
                    onClick()
                    menu?.show(anchor = it)
                }

                setIcon(icon, contentDescription)
            }
        },
    )
}

private class SearchSelector @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0,
) : RelativeLayout(context, attrs, defStyle) {

    private val binding = SearchSelectorBinding.inflate(LayoutInflater.from(context), this)

    fun setIcon(icon: Drawable?, contentDescription: String?) {
        binding.icon.setImageDrawable(icon)
        binding.icon.contentDescription = contentDescription
    }
}

@PreviewLightDark
@Composable
private fun SearchSelectorPreview() {
    AcornTheme {
        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
            SearchSelector(
                painter = painterResource(iconsR.drawable.mozac_ic_search_24),
                tint = AcornTheme.colors.iconPrimary,
                onClick = {},
            )

            SearchSelector(
                onClick = {},
                icon = getDrawable(LocalContext.current, iconsR.drawable.mozac_ic_search_24),
            )
        }
    }
}
