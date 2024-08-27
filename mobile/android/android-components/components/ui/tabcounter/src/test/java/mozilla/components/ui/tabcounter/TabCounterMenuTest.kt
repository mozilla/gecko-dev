import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.ui.tabcounter.TabCounterMenu
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class TabCounterMenuTest {

    @Test
    fun `return only the new tab item`() {
        var testItem: TabCounterMenu.Item? = null
        val onItemTapped: (TabCounterMenu.Item) -> Unit = { testItem = it }
        val menu = TabCounterMenu(testContext, onItemTapped)

        val item = menu.newTabItem
        assertEquals("New tab", item.text)
        item.onClick()

        assertEquals(TabCounterMenu.Item.NewTab, testItem)
    }

    @Test
    fun `return only the new private tab item`() {
        var testItem: TabCounterMenu.Item? = null
        val onItemTapped: (TabCounterMenu.Item) -> Unit = { testItem = it }
        val menu = TabCounterMenu(testContext, onItemTapped)

        val item = menu.newPrivateTabItem
        assertEquals("New private tab", item.text)
        item.onClick()

        assertEquals(TabCounterMenu.Item.NewPrivateTab, testItem)
    }

    @Test
    fun `return a close button`() {
        var testItem: TabCounterMenu.Item? = null
        val onItemTapped: (TabCounterMenu.Item) -> Unit = { testItem = it }
        val menu = TabCounterMenu(testContext, onItemTapped)

        val item = menu.closeTabItem
        assertEquals("Close tab", item.text)
        item.onClick()

        assertEquals(TabCounterMenu.Item.CloseTab, testItem)
    }
}
