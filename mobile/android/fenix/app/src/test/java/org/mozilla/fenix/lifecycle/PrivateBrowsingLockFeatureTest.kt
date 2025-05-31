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
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState

@RunWith(AndroidJUnit4::class)
class PrivateBrowsingLockFeatureTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    // zero tabs cases
    @Test
    fun `GIVEN feature is enabled and mode is normal WHEN number of private tabs reaches zero THEN we unlock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Normal

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertTrue(appStore.state.isPrivateScreenLocked)

        browserStore.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is enabled and mode is private WHEN authenticated and number of private tabs reaches zero THEN private mode is unchanged`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val feature = createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        feature.onSuccessfulAuthentication()
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)

        browserStore.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN the feature is disabled and mode is normal WHEN number of private tabs reaches zero THEN private mode is unchanged `() {
        val isFeatureEnabled = false
        val mode = BrowsingMode.Normal

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)

        browserStore.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN the feature is disabled and mode is private WHEN number of private tabs reaches zero THEN private mode is unchanged `() {
        val isFeatureEnabled = false
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)

        browserStore.dispatch(TabListAction.RemoveAllPrivateTabsAction).joinBlocking()
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    // initializing cases
    @Test
    fun `GIVEN feature is enabled and mode is private and there are private tabs WHEN initializing feature THEN we lock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
        BrowserState(
            tabs = listOf(
                createTab("https://www.firefox.com", id = "firefox", private = true),
                createTab("https://www.mozilla.org", id = "mozilla"),
            ),
            selectedTabId = "mozilla",
        ),
        )
        createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertTrue(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is enabled and mode is normal and there are private tabs WHEN initializing feature THEN we lock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Normal

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertTrue(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is disabled and mode is private and there are private tabs WHEN initializing the app THEN we do not lock private mode`() {
        val isFeatureEnabled = false
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is disabled and mode is normal and there are private tabs WHEN initializing the app THEN we do not lock private mode`() {
        val isFeatureEnabled = false
        val mode = BrowsingMode.Normal

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is enabled and mode is private and there are no private tabs WHEN initializing feature THEN we do not lock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is enabled and mode is normal and there are no private tabs WHEN initializing feature THEN we do not lock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Normal

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    // observing private mode tests
    @Test
    fun `GIVEN normal mode and enabled lock WHEN lifecycle is resumed THEN observing lock doesn't trigger`() {
        val localScope = TestScope()
        val mode = BrowsingMode.Normal
        val isPrivateScreenLocked = true
        var result = false
        val appStore = AppStore(initialState = AppState(mode = mode, isPrivateScreenLocked = isPrivateScreenLocked))

        observePrivateModeLock(
            viewLifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
            scope = localScope,
            appStore = appStore,
            onPrivateModeLocked = { result = true },
        )
        localScope.advanceUntilIdle()

        assertFalse(result)
    }

    @Test
    fun `GIVEN normal mode and disabled lock WHEN lifecycle is resumed THEN observing lock doesn't trigger`() {
        val localScope = TestScope()
        val mode = BrowsingMode.Normal
        val isPrivateScreenLocked = false
        var result = false
        val appStore = AppStore(initialState = AppState(mode = mode, isPrivateScreenLocked = isPrivateScreenLocked))

        observePrivateModeLock(
            viewLifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
            scope = localScope,
            appStore = appStore,
            onPrivateModeLocked = { result = true },
        )

        localScope.advanceUntilIdle()

        assertFalse(result)
    }

    @Test
    fun `GIVEN private mode and enabled lock WHEN lifecycle is resumed THEN observing lock triggers`() {
        val localScope = TestScope()
        val mode = BrowsingMode.Private
        val isPrivateScreenLocked = true
        var result = false
        val appStore = AppStore(initialState = AppState(mode = mode, isPrivateScreenLocked = isPrivateScreenLocked))

        observePrivateModeLock(
            viewLifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
            scope = localScope,
            appStore = appStore,
            onPrivateModeLocked = { result = true },
        )

        localScope.advanceUntilIdle()

        assertTrue(result)
    }

    @Test
    fun `GIVEN private mode and disabled lock WHEN lifecycle is resumed THEN observing lock doesn't trigger`() {
        val localScope = TestScope()
        val mode = BrowsingMode.Private
        val isPrivateScreenLocked = false
        var result = false
        val appStore = AppStore(initialState = AppState(mode = mode, isPrivateScreenLocked = isPrivateScreenLocked))

        observePrivateModeLock(
            viewLifecycleOwner = MockedLifecycleOwner(Lifecycle.State.RESUMED),
            scope = localScope,
            appStore = appStore,
            onPrivateModeLocked = { result = true },
        )

        localScope.advanceUntilIdle()

        assertFalse(result)
    }

    // on stop tests
    @Test
    fun `GIVEN feature is on and mode is private and there are unlocked private tabs WHEN Activity stops without config change THEN we lock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val feature = createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        // imitate user passing auth
        appStore.dispatch(AppAction.PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(isLocked = false)).joinBlocking()
        appStore.waitUntilIdle()

        assertTrue(appStore.state.mode == mode)
        assertFalse(appStore.state.isPrivateScreenLocked)

        val activity = mockk<AppCompatActivity>(relaxed = true)
        every { activity.isChangingConfigurations } returns false

        feature.onStop(activity)
        appStore.waitUntilIdle()

        assertTrue(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is on and mode is normal and there are unlocked private tabs WHEN Activity stops without config change THEN we lock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Normal

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val feature = createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        // imitate user passing auth
        appStore.dispatch(AppAction.PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(isLocked = false)).joinBlocking()
        appStore.waitUntilIdle()

        assertTrue(appStore.state.mode == mode)
        assertFalse(appStore.state.isPrivateScreenLocked)

        val activity = mockk<AppCompatActivity>(relaxed = true)
        every { activity.isChangingConfigurations } returns false

        feature.onStop(activity)
        appStore.waitUntilIdle()

        assertTrue(appStore.state.mode == mode)
        assertTrue(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is on and mode is private and there are no private tabs WHEN Activity stops without config change THEN we do not lock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val feature = createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertTrue(appStore.state.mode == mode)
        assertFalse(appStore.state.isPrivateScreenLocked)

        val activity = mockk<AppCompatActivity>(relaxed = true)
        every { activity.isChangingConfigurations } returns false

        feature.onStop(activity)
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is on and mode is normal and there are no private tabs WHEN Activity stops without config change THEN we do not lock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Normal

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val feature = createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertTrue(appStore.state.mode == mode)
        assertFalse(appStore.state.isPrivateScreenLocked)

        val activity = mockk<AppCompatActivity>(relaxed = true)
        every { activity.isChangingConfigurations } returns false

        feature.onStop(activity)
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is off and mode is private and there are private tabs WHEN Activity stops without config change THEN we do not lock private mode`() {
        val isFeatureEnabled = false
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val feature = createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertTrue(appStore.state.mode == mode)
        assertFalse(appStore.state.isPrivateScreenLocked)

        val activity = mockk<AppCompatActivity>(relaxed = true)
        every { activity.isChangingConfigurations } returns false

        feature.onStop(activity)
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN feature is off and mode is normal and there are private tabs WHEN Activity stops without config change THEN we do not lock private mode`() {
        val isFeatureEnabled = false
        val mode = BrowsingMode.Normal

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val feature = createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        assertTrue(appStore.state.mode == mode)
        assertFalse(appStore.state.isPrivateScreenLocked)

        val activity = mockk<AppCompatActivity>(relaxed = true)
        every { activity.isChangingConfigurations } returns false

        feature.onStop(activity)
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    // NB: this is an important case: we don't want to lock the screen on config change if the user is already in
    // private mode because switching modes causes the activity to recreate; e.g., the user switches to private mode
    // tab through the tabstray. the app goes into private mode and then activity shuts due to configuration change.
    // if we lock the screen at that point, going into private mode will always lock it due to activity restart.
    @Test
    fun `GIVEN feature is on and mode is private and there are private tabs WHEN Activity stops with config change THEN we do not lock private mode`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val feature = createFeature(browserStore = browserStore, appStore = appStore, storage = createStorage(isFeatureEnabled = isFeatureEnabled))
        appStore.waitUntilIdle()

        feature.onSuccessfulAuthentication()
        appStore.waitUntilIdle()

        val activity = mockk<AppCompatActivity>(relaxed = true)
        every { activity.isChangingConfigurations } returns true

        feature.onStop(activity)
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    // turning the feature on and off tests
    @Test
    fun `GIVEN the feature is on and there are private tabs WHEN we turn off the feature THEN we unlock private tabs and don't lock it`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val storage = createStorage(isFeatureEnabled = isFeatureEnabled)

        val feature = createFeature(browserStore = browserStore, appStore = appStore, storage = storage)
        appStore.waitUntilIdle()

        assertTrue(appStore.state.isPrivateScreenLocked)

        // verify that disabled feature state unlocks private mode
        val sharedPrefUpdate = false
        storage.listener?.invoke(sharedPrefUpdate)
        appStore.waitUntilIdle()

        assertTrue(browserStore.state.privateTabs.isNotEmpty())
        assertFalse(appStore.state.isPrivateScreenLocked)

        // verify that activity.onStop doesn't lock private mode
        val activity = mockk<AppCompatActivity>(relaxed = true)
        every { activity.isChangingConfigurations } returns false

        feature.onStop(activity)
        appStore.waitUntilIdle()

        assertTrue(browserStore.state.privateTabs.isNotEmpty())
        assertFalse(appStore.state.isPrivateScreenLocked)

        // verify that going to normal mode doesn't lock private mode
        appStore.dispatch(AppAction.ModeChange(mode = BrowsingMode.Normal)).joinBlocking()

        assertTrue(browserStore.state.privateTabs.isNotEmpty())
        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN the feature is off and mode is normal and there are private tabs WHEN we turn on the feature THEN we lock private tabs`() {
        val isFeatureEnabled = false
        val mode = BrowsingMode.Normal

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val storage = createStorage(isFeatureEnabled = isFeatureEnabled)

        createFeature(browserStore = browserStore, appStore = appStore, storage = storage)
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)

        // verify that disabled feature state unlocks private mode
        val sharedPrefUpdate = true
        storage.listener?.invoke(sharedPrefUpdate)
        appStore.waitUntilIdle()

        assertTrue(browserStore.state.privateTabs.isNotEmpty())
        assertTrue(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `GIVEN the feature is off and mode is private and there are private tabs WHEN we turn on the feature THEN we do not lock private tabs`() {
        val isFeatureEnabled = false
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val storage = createStorage(isFeatureEnabled = isFeatureEnabled)

        createFeature(browserStore = browserStore, appStore = appStore, storage = storage)
        appStore.waitUntilIdle()

        assertFalse(appStore.state.isPrivateScreenLocked)

        // verify that disabled feature state unlocks private mode
        val sharedPrefUpdate = true
        storage.listener?.invoke(sharedPrefUpdate)
        appStore.waitUntilIdle()

        assertTrue(browserStore.state.privateTabs.isNotEmpty())
        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    // turning the feature on and off tests
    @Test
    fun `GIVEN the feature is on and there are private tabs WHEN switching to normal mode THEN we unlock private tabs and don't lock it`() {
        val isFeatureEnabled = true
        val mode = BrowsingMode.Private

        val appStore = AppStore(initialState = AppState(mode = mode))
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("https://www.firefox.com", id = "firefox", private = true),
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
                selectedTabId = "mozilla",
            ),
        )
        val storage = createStorage(isFeatureEnabled = isFeatureEnabled)

        createFeature(browserStore = browserStore, appStore = appStore, storage = storage)
        appStore.waitUntilIdle()

        assertTrue(appStore.state.isPrivateScreenLocked)

        // imitate user passing auth
        appStore.dispatch(AppAction.PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(isLocked = false)).joinBlocking()
        appStore.waitUntilIdle()

        assertTrue(appStore.state.mode == BrowsingMode.Private)
        assertFalse(appStore.state.isPrivateScreenLocked)

        // verify that going to normal mode doesn't lock private mode
        appStore.dispatch(AppAction.ModeChange(mode = BrowsingMode.Normal)).joinBlocking()
        appStore.waitUntilIdle()

        assertTrue(appStore.state.mode == BrowsingMode.Normal)
        assertFalse(appStore.state.isPrivateScreenLocked)
    }

    @Test
    fun `WHEN feature resumes THEN storage is listening to shared pref`() {
        val storage = MockedPrivateBrowsingLockStorage()
        val feature = createFeature(browserStore = BrowserStore(), appStore = AppStore(), storage = storage)
        val lifecycleOwner = MockedLifecycleOwner(Lifecycle.State.CREATED)

        assertFalse(storage.startCalled)

        feature.onResume(lifecycleOwner)
        assertTrue(storage.startCalled)
    }

    internal class MockedLifecycleOwner(initialState: Lifecycle.State) : LifecycleOwner {
        override val lifecycle: Lifecycle = LifecycleRegistry(this).apply {
            currentState = initialState
        }
    }

    internal class MockedPrivateBrowsingLockStorage(
        override val isFeatureEnabled: Boolean = true,
    ) : PrivateBrowsingLockStorage {
        var listener: ((Boolean) -> Unit)? = null

        var startCalled = false

        override fun addFeatureStateListener(listener: (Boolean) -> Unit) {
            this.listener = listener
        }

        override fun startObservingSharedPrefs() {
            startCalled = true
        }
    }

    private fun createFeature(
        appStore: AppStore,
        browserStore: BrowserStore,
        storage: PrivateBrowsingLockStorage,
    ) = PrivateBrowsingLockFeature(
        appStore = appStore,
        browserStore = browserStore,
        storage = storage,
    )

    private fun createStorage(isFeatureEnabled: Boolean = true) = MockedPrivateBrowsingLockStorage(isFeatureEnabled)
}
