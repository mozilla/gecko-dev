/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks.ui

import org.junit.Assert.assertEquals
import org.junit.Test

class BookmarksReducerKtTest {
    @Test
    fun `WHEN store initializes THEN no changes to state`() {
        val state = BookmarksState.default

        assertEquals(state, bookmarksReducer(state, Init))
    }

    @Test
    fun `WHEN bookmarks are loaded THEN they are added to state`() {
        val state = BookmarksState.default
        val items = List(5) {
            BookmarkItem.Folder("$it")
        }

        val result = bookmarksReducer(state, BookmarksLoaded(items))

        assertEquals(state.copy(bookmarkItems = items), result)
    }
}
