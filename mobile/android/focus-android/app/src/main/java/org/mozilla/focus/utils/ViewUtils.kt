/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import android.view.MenuItem
import android.view.View
import androidx.annotation.StringRes
import com.google.android.material.snackbar.Snackbar

object ViewUtils {

    private const val MENU_ITEM_ALPHA_ENABLED = 255
    private const val MENU_ITEM_ALPHA_DISABLED = 130

    /**
     * Create a custom FocusSnackbar.
     */
    fun showBrandedSnackbar(view: View?, @StringRes resId: Int, delayMillis: Int) {
        val context = view!!.context
        val snackbar = FocusSnackbar.make(view, Snackbar.LENGTH_LONG)
        snackbar.setText(context.getString(resId))

        view.postDelayed({ snackbar.show() }, delayMillis.toLong())
    }

    /**
     * Enable or disable a [MenuItem]
     * If the menu item is disabled it can not be clicked and the menu icon is semi-transparent
     *
     * @param menuItem the menu item to enable/disable
     * @param enabled true if the menu item should be enabled
     */
    fun setMenuItemEnabled(menuItem: MenuItem, enabled: Boolean) {
        menuItem.isEnabled = enabled
        val icon = menuItem.icon
        if (icon != null) {
            icon.mutate().alpha = if (enabled) MENU_ITEM_ALPHA_ENABLED else MENU_ITEM_ALPHA_DISABLED
        }
    }
}
