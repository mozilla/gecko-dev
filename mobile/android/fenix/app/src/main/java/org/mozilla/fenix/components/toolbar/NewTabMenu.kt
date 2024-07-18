/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.ui.tabcounter.TabCounterMenu

/**
 * An implementation of [TabCounterMenu] used to display a menu for the new tab button.
 */
class NewTabMenu(
    context: Context,
    onItemTapped: (Item) -> Unit,
    iconColor: Int? = null,
) : TabCounterMenu(context, onItemTapped, iconColor) {

    init {
        menuController.submitList(menuItems())
    }

    @VisibleForTesting
    internal fun menuItems(): List<MenuCandidate> = listOf(
        newTabItem,
        newPrivateTabItem,
    )
}
