/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lifecycle

import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import io.mockk.every
import io.mockk.mockk
import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import kotlinx.coroutines.test.TestScope
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.PrivateBrowsingLockAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.tabstray.TabsTrayFragment
import org.mozilla.fenix.utils.Settings

@RunWith(AndroidJUnit4::class)
class PrivateBrowsingLockFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private lateinit var appStore: AppStore
    private lateinit var browserStore: BrowserStore
    private lateinit var settings: Settings

    @Before
    fun setup() {
        appStore = spy(AppStore())
        browserStore = BrowserStore()
        settings = mockk(relaxed = true)
    }

    @Test
    fun `GIVEN the feature is enabled WHEN number of private tabs reaches zero THEN we unlock private mode`() {
        every { settings.privateBrowsingLockedEnabled } returns true
        val store = createBrowser()
        PrivateBrowsingLockFeature(appStore, store, settings)

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        verify(appStore).dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = false,
            ),
        )
    }

    @Test
    fun `GIVEN the feature is disabled WHEN number of private tabs reaches zero THEN we don't lock private mode`() {
        every { settings.privateBrowsingLockedEnabled } returns false
        val store = createBrowser()
        PrivateBrowsingLockFeature(appStore, store, settings)

        store.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()

        verify(appStore, never()).dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = false,
            ),
        )
    }

    @Test
    fun `GIVEN the feature is on and there are private tabs WHEN initializing the app THEN we lock private mode`() {
        every { settings.privateBrowsingLockedEnabled } returns true
        val store = createBrowser(
            tabs = listOf(
                createTab("https://www.firefox.com", id = "firefox", private = true),
                createTab("https://www.mozilla.org", id = "mozilla"),
            ),
            selectedTabId = "mozilla",
        )
        val appStore = spy(
            AppStore(
                initialState = AppState(mode = BrowsingMode.Private),
            ),
        )

        PrivateBrowsingLockFeature(appStore, store, settings)

        verify(appStore).dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = true,
            ),
        )
    }

    @Test
    fun `GIVEN the feature is off and there are private tabs WHEN initializing the app THEN we do not lock private mode`() {
        every { settings.privateBrowsingLockedEnabled } returns false
        val store = createBrowser(
            tabs = listOf(
                createTab("https://www.firefox.com", id = "firefox", private = true),
                createTab("https://www.mozilla.org", id = "mozilla"),
            ),
            selectedTabId = "mozilla",
        )
        val appStore = spy(
            AppStore(
                initialState = AppState(mode = BrowsingMode.Private),
            ),
        )

        PrivateBrowsingLockFeature(appStore, store, settings)

        verify(appStore, times(0)).dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = true,
            ),
        )
    }

    @Test
    fun `GIVEN there are private tabs WHEN switching to normal mode THEN we lock private mode`() {
        every { settings.privateBrowsingLockedEnabled } returns true
        val store = createBrowser(tabs = listOf(createTab("https://www.mozilla.org", id = "mozilla")))
        val appStore = spy(AppStore(initialState = AppState(mode = BrowsingMode.Private)))
        PrivateBrowsingLockFeature(appStore, store, settings)

        store.dispatch(TabListAction.AddTabAction(createTab("https://www.firefox.com", id = "firefox", private = true))).joinBlocking()
        appStore.dispatch(AppAction.ModeChange(mode = BrowsingMode.Normal)).joinBlocking()

        verify(appStore, times(1)).dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = true,
            ),
        )
    }

    @Test
    fun `GIVEN normal mode and enabled lock WHEN lifecycle is resumed THEN observing lock doesn't trigger`() {
        val localScope = TestScope()
        var isLocked = false
        observePrivateModeLock(
            viewLifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
            scope = localScope,
            appStore = appStore,
            onPrivateModeLocked = { isLocked = true },
        )

        appStore.dispatch(AppAction.ModeChange(mode = BrowsingMode.Normal)).joinBlocking()
        appStore.dispatch(PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(true)).joinBlocking()
        localScope.advanceUntilIdle()

        assertFalse(isLocked)
    }

    @Test
    fun `GIVEN normal mode and disabled lock WHEN lifecycle is resumed THEN observing lock doesn't trigger`() {
        val localScope = TestScope()
        var isLocked = false
        observePrivateModeLock(
            viewLifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
            scope = localScope,
            appStore = appStore,
            onPrivateModeLocked = { isLocked = true },
        )

        appStore.dispatch(AppAction.ModeChange(mode = BrowsingMode.Normal)).joinBlocking()
        appStore.dispatch(PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(true)).joinBlocking()
        localScope.advanceUntilIdle()

        assertFalse(isLocked)
    }

    @Test
    fun `GIVEN private mode and enabled lock WHEN lifecycle is resumed THEN observing lock triggers`() {
        val localScope = TestScope()
        var isLocked = false
        observePrivateModeLock(
            viewLifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
            scope = localScope,
            appStore = appStore,
            onPrivateModeLocked = { isLocked = true },
        )

        appStore.dispatch(AppAction.ModeChange(mode = BrowsingMode.Private)).joinBlocking()
        appStore.dispatch(PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(true)).joinBlocking()
        localScope.advanceUntilIdle()

        assertTrue(isLocked)
    }

    @Test
    fun `GIVEN private mode and disabled lock WHEN lifecycle is resumed THEN observing lock doesn't trigger`() {
        val localScope = TestScope()
        var isLocked = false
        observePrivateModeLock(
            viewLifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
            scope = localScope,
            appStore = appStore,
            onPrivateModeLocked = { isLocked = true },
        )

        appStore.dispatch(PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(false)).joinBlocking()
        appStore.dispatch(AppAction.ModeChange(mode = BrowsingMode.Private)).joinBlocking()
        localScope.advanceUntilIdle()

        assertFalse(isLocked)
    }

    @Test
    fun `GIVEN private mode with private tabs WHEN Activity stops without config change THEN lock is triggered`() {
        every { settings.privateBrowsingLockedEnabled } returns true
        val store = createBrowser(
            tabs = listOf(createTab("https://www.mozilla.org", id = "mozilla")),
            selectedTabId = "mozilla",
        )
        val appStore = spy(AppStore(AppState(mode = BrowsingMode.Private)))
        val feature = PrivateBrowsingLockFeature(appStore, store, settings)
        val activity = mockk<AppCompatActivity>(relaxed = true)
        every { activity.isChangingConfigurations } returns false

        store.dispatch(TabListAction.AddTabAction(createTab("https://www.firefox.com", id = "firefox", private = true))).joinBlocking()
        feature.onStop(activity)

        verify(appStore).dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(isLocked = true),
        )
    }

    @Test
    fun `GIVEN normal mode with private tabs WHEN TabsTrayFragment stops THEN lock is triggered`() {
        every { settings.privateBrowsingLockedEnabled } returns true
        val store = createBrowser(
            tabs = listOf(createTab("https://www.mozilla.org", id = "mozilla")),
            selectedTabId = "mozilla",
        )
        val appStore = spy(AppStore(AppState(mode = BrowsingMode.Normal)))
        val feature = PrivateBrowsingLockFeature(appStore, store, settings)
        val fragment = mockk<TabsTrayFragment>(relaxed = true)

        store.dispatch(TabListAction.AddTabAction(createTab("https://www.firefox.com", id = "firefox", private = true))).joinBlocking()
        feature.onStop(fragment)

        verify(appStore).dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(isLocked = true),
        )
    }

    internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        override val lifecycle: Lifecycle = LifecycleRegistry(this).apply {
            currentState = initialState
        }
    }

    private fun createBrowser(
        tabs: List<TabSessionState> = listOf(
            createTab("https://www.firefox.com", id = "firefox", private = true),
            createTab("https://www.mozilla.org", id = "mozilla"),
        ),
        selectedTabId: String = "mozilla",
    ): BrowserStore {
        return BrowserStore(
            BrowserState(
                tabs = tabs,
                selectedTabId = selectedTabId,
            ),
        )
    }
}
