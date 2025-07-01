/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.browser.browsingmode

import android.content.Intent
import io.mockk.MockKAnnotations
import io.mockk.Runs
import io.mockk.every
import io.mockk.impl.annotations.MockK
import io.mockk.just
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity.Companion.PRIVATE_BROWSING_MODE
import org.mozilla.fenix.helpers.MockkRetryTestRule
import org.mozilla.fenix.utils.Settings
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class DefaultBrowsingModeManagerTest {

    @MockK lateinit var settings: Settings

    @MockK(relaxed = true)
    lateinit var modeDidChange: (BrowsingMode) -> Unit

    @MockK(relaxed = true)
    lateinit var updateAppStateMode: (BrowsingMode) -> Unit

    @get:Rule
    val mockkRule = MockkRetryTestRule()

    @Before
    fun before() {
        MockKAnnotations.init(this)

        every { settings.lastKnownMode = any() } just Runs
        every { settings.lastKnownMode } returns BrowsingMode.Normal
    }

    @Test
    fun `WHEN mode is set THEN modeDidChange and updateAppState callbacks are invoked and last known mode setting is set`() {
        val manager = buildBrowsingModeManger()

        verify(exactly = 0) {
            modeDidChange.invoke(any())
            settings.lastKnownMode = any()
            updateAppStateMode.invoke(any())
        }

        manager.mode = BrowsingMode.Private

        verify {
            modeDidChange(BrowsingMode.Private)
            settings.lastKnownMode = BrowsingMode.Private
            updateAppStateMode(BrowsingMode.Private)
        }

        manager.mode = BrowsingMode.Normal
        manager.mode = BrowsingMode.Normal

        verify {
            modeDidChange(BrowsingMode.Normal)
            settings.lastKnownMode = BrowsingMode.Normal
            updateAppStateMode(BrowsingMode.Normal)
        }
    }

    @Test
    fun `WHEN mode is set THEN it should be returned from get`() {
        val manager = buildBrowsingModeManger()

        assertEquals(BrowsingMode.Normal, manager.mode)

        manager.mode = BrowsingMode.Private

        assertEquals(BrowsingMode.Private, manager.mode)
        verify { settings.lastKnownMode = BrowsingMode.Private }

        manager.mode = BrowsingMode.Normal

        assertEquals(BrowsingMode.Normal, manager.mode)
        verify { settings.lastKnownMode = BrowsingMode.Normal }
    }

    @Test
    fun `GIVEN browsing mode is not set by intent and private mode with a tab persisted WHEN browsing mode manager is initialized THEN set browsing mode to private`() {
        every { settings.lastKnownMode } returns BrowsingMode.Private

        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(url = "https://mozilla.org", private = true),
                ),
            ),
        )
        val manager = buildBrowsingModeManger(store = browserStore)

        assertEquals(BrowsingMode.Private, manager.mode)
    }

    @Test
    fun `GIVEN last known mode is private mode and no tabs persisted WHEN browsing mode manager is initialized THEN set browsing mode to normal`() {
        every { settings.lastKnownMode } returns BrowsingMode.Private

        val manager = buildBrowsingModeManger()

        assertEquals(BrowsingMode.Normal, manager.mode)
    }

    @Test
    fun `GIVEN last known mode is normal mode WHEN browsing mode manager is initialized THEN set browsing mode to normal`() {
        every { settings.lastKnownMode } returns BrowsingMode.Normal

        val manager = buildBrowsingModeManger()

        assertEquals(BrowsingMode.Normal, manager.mode)
    }

    @Test
    fun `GIVEN private browsing mode intent WHEN browsing mode manager is initialized THEN set browsing mode to private`() {
        val intent = Intent()
        intent.putExtra(PRIVATE_BROWSING_MODE, true)

        val manager = buildBrowsingModeManger(intent = intent)

        assertEquals(BrowsingMode.Private, manager.mode)
    }

    @Test
    fun `GIVEN private browsing mode intent WHEN update mode is called THEN set browsing mode to private`() {
        val intent = Intent()
        intent.putExtra(PRIVATE_BROWSING_MODE, true)

        val manager = buildBrowsingModeManger()

        assertEquals(BrowsingMode.Normal, manager.mode)

        manager.updateMode(intent)

        assertEquals(BrowsingMode.Private, manager.mode)
    }

    @Test
    fun `GIVEN browsing mode is not set by intent and private mode with a tab persisted WHEN update mode is called THEN set browsing mode to private`() {
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(url = "https://mozilla.org", private = true),
                ),
            ),
        )
        val manager = buildBrowsingModeManger(store = browserStore)

        assertEquals(BrowsingMode.Normal, manager.mode)

        every { settings.lastKnownMode } returns BrowsingMode.Private

        manager.updateMode()

        assertEquals(BrowsingMode.Private, manager.mode)
    }

    @Test
    fun `GIVEN browsing mode is not set by intent and private mode and no tabs persisted WHEN update mode is called THEN set browsing mode to normal`() {
        every { settings.lastKnownMode } returns BrowsingMode.Private

        val manager = buildBrowsingModeManger()

        assertEquals(BrowsingMode.Normal, manager.mode)

        manager.updateMode()

        assertEquals(BrowsingMode.Normal, manager.mode)
    }

    @Test
    fun `GIVEN last known mode is normal mode WHEN update mode is called THEN set browsing mode to normal`() {
        every { settings.lastKnownMode } returns BrowsingMode.Normal

        val manager = buildBrowsingModeManger()

        assertEquals(BrowsingMode.Normal, manager.mode)

        manager.updateMode()

        assertEquals(BrowsingMode.Normal, manager.mode)
    }

    private fun buildBrowsingModeManger(
        intent: Intent? = null,
        store: BrowserStore = BrowserStore(),
    ): BrowsingModeManager {
        return DefaultBrowsingModeManager(
            intent = intent,
            store = store,
            settings = settings,
            modeDidChange = modeDidChange,
            updateAppStateMode = updateAppStateMode,
        )
    }
}
