/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.toolbar

import android.content.Context
import androidx.appcompat.view.ContextThemeWrapper
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class NewTabMenuTest {

    private lateinit var context: Context
    private lateinit var onItemTapped: (TabCounterMenu.Item) -> Unit
    private lateinit var menu: NewTabMenu

    @Before
    fun setup() {
        context = ContextThemeWrapper(testContext, R.style.NormalTheme)
        onItemTapped = mock()
        menu = NewTabMenu(context, onItemTapped)
    }

    @Test
    fun `WHEN menu items getter is called THEN return the new tab and private tab menu item`() {
        val items = menu.menuItems()
        val newTab = items[0] as TextMenuCandidate
        val newPrivateTab = items[1] as TextMenuCandidate

        assertEquals(2, items.size)
        assertEquals("New tab", newTab.text)
        assertEquals("New private tab", newPrivateTab.text)

        newTab.onClick()
        verify(onItemTapped).invoke(TabCounterMenu.Item.NewTab)

        newPrivateTab.onClick()
        verify(onItemTapped).invoke(TabCounterMenu.Item.NewPrivateTab)
    }
}
