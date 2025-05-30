/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.support.ktx.android.content.getColorFromAttr
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.R
import org.mozilla.fenix.library.bookmarks.BookmarkItemMenu.Item
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class BookmarkItemMenuTest {

    private lateinit var context: Context
    private lateinit var menu: BookmarkItemMenu
    private lateinit var bookmarksStorage: BookmarksStorage

    private var lastItemTapped: Item? = null
    private val item = BookmarkNode(BookmarkNodeType.ITEM, "456", "123", 0u, "Mozilla", "http://mozilla.org", 0, 0, null)
    private val emptyFolder = BookmarkNode(BookmarkNodeType.FOLDER, "987", "123", 0u, "Subfolder", null, 0, 0, emptyList())
    private val notEmptyFolder = BookmarkNode(BookmarkNodeType.FOLDER, "987", "123", 0u, "Subfolder", null, 0, 0, listOf(item))

    @Before
    fun setup() {
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        bookmarksStorage = mockk()
        menu = BookmarkItemMenu(context, bookmarksStorage) { lastItemTapped = it }
    }

    @Test
    fun `delete item has special styling`() = runTest {
        val deleteItem = menu.menuItems(BookmarkNodeType.SEPARATOR, "").last()

        assertNotNull(deleteItem)
        assertEquals("Delete", deleteItem.text)
        assertEquals(
            TextStyle(color = context.getColorFromAttr(R.attr.textCritical)),
            deleteItem.textStyle,
        )

        deleteItem.onClick()

        assertEquals(Item.Delete, lastItemTapped)
    }

    @Test
    fun `edit item appears for folders`() = runTest {
        // empty folder
        coEvery { bookmarksStorage.getTree(any()) } returns emptyFolder

        var emptyFolderItems = menu.menuItems(BookmarkNodeType.FOLDER, "")

        assertNotNull(emptyFolderItems)
        assertEquals(2, emptyFolderItems.size)

        // not empty folder
        coEvery { bookmarksStorage.getTree(any()) } returns notEmptyFolder
        var folderItems: List<TextMenuCandidate>? = null

        folderItems = menu.menuItems(BookmarkNodeType.FOLDER, "")

        assertNotNull(folderItems)
        assertEquals(4, folderItems.size)

        val (edit, openAll, openAllPrivate, delete) = folderItems

        assertEquals("Edit", edit.text)
        assertEquals("Open all in new tabs", openAll.text)
        assertEquals("Open all in private tabs", openAllPrivate.text)
        assertEquals("Delete", delete.text)

        edit.onClick()
        assertEquals(Item.Edit, lastItemTapped)

        openAll.onClick()
        assertEquals(Item.OpenAllInNewTabs, lastItemTapped)

        openAllPrivate.onClick()
        assertEquals(Item.OpenAllInPrivateTabs, lastItemTapped)

        delete.onClick()
        assertEquals(Item.Delete, lastItemTapped)
    }

    @Test
    fun `all item appears for sites`() = runTest {
        val siteItems = menu.menuItems(BookmarkNodeType.ITEM, "")

        assertNotNull(siteItems)
        assertEquals(6, siteItems.size)
        val (edit, copy, share, openInNewTab, openInPrivateTab, delete) = siteItems

        assertEquals("Edit", edit.text)
        assertEquals("Copy", copy.text)
        assertEquals("Share", share.text)
        assertEquals("Open in new tab", openInNewTab.text)
        assertEquals("Open in private tab", openInPrivateTab.text)
        assertEquals("Delete", delete.text)

        edit.onClick()
        assertEquals(Item.Edit, lastItemTapped)

        copy.onClick()
        assertEquals(Item.Copy, lastItemTapped)

        share.onClick()
        assertEquals(Item.Share, lastItemTapped)

        openInNewTab.onClick()
        assertEquals(Item.OpenInNewTab, lastItemTapped)

        openInPrivateTab.onClick()
        assertEquals(Item.OpenInPrivateTab, lastItemTapped)

        delete.onClick()
        assertEquals(Item.Delete, lastItemTapped)
    }

    @Test
    fun `checkAtLeastOneChild is called only for FOLDER type`() = runTest {
        coEvery { bookmarksStorage.getTree(any()) } returns emptyFolder

        menu.menuItems(BookmarkNodeType.SEPARATOR, "")
        coVerify(exactly = 0) { bookmarksStorage.getTree(any()) }

        menu.menuItems(BookmarkNodeType.ITEM, "")
        coVerify(exactly = 0) { bookmarksStorage.getTree(any()) }

        menu.menuItems(BookmarkNodeType.FOLDER, "")
        coVerify { bookmarksStorage.getTree(any()) }
    }

    private operator fun <T> List<T>.component6(): T {
        return get(5)
    }
}
