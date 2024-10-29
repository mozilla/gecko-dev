/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar.ui

import android.content.Context
import androidx.appcompat.content.res.AppCompatResources
import mozilla.components.browser.toolbar.BrowserToolbar
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.BrowserFragment.Companion.SHARE_WEIGHT
import org.mozilla.fenix.theme.ThemeManager

/**
 * Creates a share button for the [BrowserToolbar].
 *
 * @param context [Context] used for accessing system resources.
 * @param listener Callback invoked when the button is clicked.
 */
fun BrowserToolbar.Companion.createShareBrowserAction(
    context: Context,
    listener: () -> Unit,
) = BrowserToolbar.Button(
    imageDrawable = AppCompatResources.getDrawable(
        context,
        R.drawable.mozac_ic_share_android_24,
    )!!,
    contentDescription = context.getString(R.string.browser_menu_share),
    weight = { SHARE_WEIGHT },
    iconTintColorResource = ThemeManager.resolveAttribute(R.attr.textPrimary, context),
    listener = listener,
)
