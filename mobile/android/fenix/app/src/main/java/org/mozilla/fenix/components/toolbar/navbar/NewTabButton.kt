/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.navbar

import android.content.Context
import android.content.res.Configuration
import android.util.AttributeSet
import android.view.LayoutInflater
import android.widget.RelativeLayout
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import mozilla.components.support.ktx.android.content.res.resolveAttribute
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.mozilla.fenix.R
import org.mozilla.fenix.compose.LongPressIconButton
import org.mozilla.fenix.compose.annotation.LightDarkPreview
import org.mozilla.fenix.databinding.NewTabButtonBinding
import org.mozilla.fenix.theme.FirefoxTheme
import org.mozilla.fenix.theme.Theme

// Interim composable for a new tab button that supports showing a menu on long press.
// With this being implemented as an AndroidView the menu can be shown as low to the bottom of the
// screen as needed. To be replaced with a fully Compose implementation in the future that use a
// DropdownMenu once https://github.com/JetBrains/compose-multiplatform/issues/1878 is resolved.

/**
 * Composable that delegates to an [AndroidView] to display a new tab button and optionally a menu.
 *
 * If a menu is provided it will be shown as low to the bottom of the screen as needed and will
 * be shown on long presses of the tab counter button irrespective of the [onLongPress] callback
 * being set or not.
 *
 * @param onClick Invoked when the button is clicked.
 * @param menu Optional menu to show when the button is long clicked.
 * @param onLongPress Optional callback for when the button is long clicked.
 */
@Composable
fun NewTabButton(
    onClick: () -> Unit,
    menu: TabCounterMenu? = null,
    onLongPress: () -> Unit = {},
) {
    // IconButton by default have ripple indication, we want to disable it for performance
    // TODO revert to IconButton after https://bugzilla.mozilla.org/show_bug.cgi?id=1911369
    LongPressIconButton(
        onClick = onClick, // This ensures the 48dp touch target for clicks.
        onLongClick = {},
        indication = null,
    ) {
        AndroidView(
            factory = { context ->
                NewTabButton(context).apply {
                    setOnClickListener {
                        onClick() // This ensures clicks in the 34dp touch target are caught.
                    }

                    menu?.let { menu ->
                        setOnLongClickListener {
                            onLongPress()
                            menu.menuController.show(anchor = it)
                            true
                        }
                    }

                    contentDescription = context.getString(R.string.library_new_tab)

                    setBackgroundResource(
                        context.theme.resolveAttribute(
                            android.R.attr.selectableItemBackgroundBorderless,
                        ),
                    )
                }
            },
            // The IconButton composable has a 48dp size and it's own ripple with a 24dp radius.
            // The NewTabButton view has it's own inherent ripple that has a bigger radius
            // so based on manual testing we set a size of 34dp for this View which would
            // ensure it's ripple matches the composable one. Otherwise there is a visible mismatch.
            modifier = Modifier.size(34.dp),
        )
    }
}

private class NewTabButton @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0,
) : RelativeLayout(context, attrs, defStyle) {

    init {
        NewTabButtonBinding.inflate(LayoutInflater.from(context), this)
    }
}

@LightDarkPreview
@Composable
private fun NewTabButtonPreview() {
    FirefoxTheme {
        Box(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(10.dp),
        ) {
            NewTabButton(
                onClick = {},
            )
        }
    }
}

@Preview(uiMode = Configuration.UI_MODE_NIGHT_YES)
@Composable
private fun NewTabButtonPrivatePreview() {
    FirefoxTheme(theme = Theme.Private) {
        Box(
            modifier = Modifier
                .background(FirefoxTheme.colors.layer1)
                .padding(10.dp),
        ) {
            NewTabButton(
                onClick = {},
            )
        }
    }
}
