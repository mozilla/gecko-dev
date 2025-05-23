package org.mozilla.fenix.ui.efficiency.tests

import org.junit.Test
import org.mozilla.fenix.ui.efficiency.helpers.BaseTest

class BookmarksTest : BaseTest() {

    @Test
    fun verifyBookmarksSectionTest() {
        on.bookmarks.navigateToPage()
    }
}
