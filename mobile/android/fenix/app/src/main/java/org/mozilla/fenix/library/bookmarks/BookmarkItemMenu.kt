/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu2.BrowserMenuController
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R

class BookmarkItemMenu(
    private val context: Context,
    private val bookmarkStorage: BookmarksStorage,
    var onItemTapped: ((Item) -> Unit)? = null,
) {

    enum class Item {
        Edit,
        Copy,
        Share,
        OpenInNewTab,
        OpenInPrivateTab,
        OpenAllInNewTabs,
        OpenAllInPrivateTabs,
        Delete,
    }

    val menuController: MenuController by lazy { BrowserMenuController() }

    @VisibleForTesting
    internal suspend fun menuItems(itemType: BookmarkNodeType, itemId: String): List<TextMenuCandidate> {
        val editMenuOption =
            TextMenuCandidate(text = context.getString(R.string.bookmark_menu_edit_button)) {
                onItemTapped?.invoke(Item.Edit)
            }

        val copyMenuOption =
            TextMenuCandidate(text = context.getString(R.string.bookmark_menu_copy_button)) {
                onItemTapped?.invoke(Item.Copy)
            }

        val deleteMenuOption =
            TextMenuCandidate(
                text = context.getString(R.string.bookmark_menu_delete_button),
                textStyle = TextStyle(color = context.getColorFromAttr(R.attr.textCritical)),
            ) {
                onItemTapped?.invoke(Item.Delete)
            }

        val shareMenuOption =
            TextMenuCandidate(text = context.getString(R.string.bookmark_menu_share_button)) {
                onItemTapped?.invoke(Item.Share)
            }

        val openInNewTabMenuOption =
            TextMenuCandidate(text = context.getString(R.string.bookmark_menu_open_in_new_tab_button)) {
                onItemTapped?.invoke(Item.OpenInNewTab)
            }

        val openInPrivateTabMenuOption =
            TextMenuCandidate(text = context.getString(R.string.bookmark_menu_open_in_private_tab_button)) {
                onItemTapped?.invoke(Item.OpenInPrivateTab)
            }

        val openAllInNewTabsMenuOption =
            TextMenuCandidate(text = context.getString(R.string.bookmark_menu_open_all_in_tabs_button)) {
                onItemTapped?.invoke(Item.OpenAllInNewTabs)
            }

        val openAllInPrivateTabsMenuOption =
            TextMenuCandidate(text = context.getString(R.string.bookmark_menu_open_all_in_private_tabs_button)) {
                onItemTapped?.invoke(Item.OpenAllInPrivateTabs)
            }

        return buildList {
            if (itemType != BookmarkNodeType.SEPARATOR) {
                add(editMenuOption)
            }
            if (itemType == BookmarkNodeType.ITEM) {
                add(copyMenuOption)
                add(shareMenuOption)
                add(openInNewTabMenuOption)
                add(openInPrivateTabMenuOption)
            }

            if (itemType == BookmarkNodeType.FOLDER && checkAtLeastOneChild(itemId)) {
                add(openAllInNewTabsMenuOption)
                add(openAllInPrivateTabsMenuOption)
            }

            add(deleteMenuOption)
        }
    }

    /**
     * Update the menu items for the type of bookmark.
     */
    suspend fun updateMenu(itemType: BookmarkNodeType, itemId: String) {
        menuController.submitList(menuItems(itemType, itemId))
    }

    /**
     * Checks if a bookmark item has at least one child.
     *
     * @param itemId The ID of the bookmark item to check.
     * @return `true` if the item has at least one child, `false` otherwise.
     */
    @VisibleForTesting
    internal suspend fun checkAtLeastOneChild(itemId: String): Boolean =
        bookmarkStorage.getTree(
            itemId,
            false,
        )?.children?.isNotEmpty() == true
}
