/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lifecycle

import android.app.Activity
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flow
import mozilla.components.lib.state.ext.flowScoped
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.PrivateBrowsingLockAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.tabstray.TabsTrayFragment
import org.mozilla.fenix.utils.Settings

/**
 * A lifecycle-aware feature that locks private browsing mode behind authentication
 * when certain conditions are met (e.g., switching modes or backgrounding the app).
 */
class PrivateBrowsingLockFeature(
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val settings: Settings,
) : DefaultLifecycleObserver {

    init {
        // When the app is initialized, if there are private tabs, we should lock the private mode.
        if (settings.privateBrowsingLockedEnabled) {
            appStore.dispatch(
                PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                    isLocked = browserStore.state.privateTabs.isNotEmpty(),
                ),
            )
        }

        observePrivateTabsClosure()
        observeSwitchingToNormalMode()
    }

    private fun observePrivateTabsClosure() {
        browserStore.flowScoped { flow ->
            flow
                .map { it.privateTabs.size }
                .distinctUntilChanged()
                .filter { it == 0 }
                .collect {
                    if (settings.privateBrowsingLockedEnabled) {
                        // When all private tabs are closed, we don't need to lock the private mode.
                        appStore.dispatch(
                            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                                isLocked = false,
                            ),
                        )
                    }
                }
        }
    }

    private fun observeSwitchingToNormalMode() {
        appStore.flowScoped { flow ->
            flow
                .map { it.mode }
                .distinctUntilChanged()
                .filter { it == BrowsingMode.Normal }
                .collect {
                    // When witching from private to normal mode with private tabs open,
                    // we lock the private mode.
                    val isPrivateModeLockEnabled = settings.privateBrowsingLockedEnabled
                    val hasPrivateTabs = browserStore.state.privateTabs.isNotEmpty()

                    if (isPrivateModeLockEnabled && hasPrivateTabs) {
                        appStore.dispatch(
                            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                                isLocked = true,
                            ),
                        )
                    }
                }
        }
    }

    override fun onStop(owner: LifecycleOwner) {
        super.onStop(owner)

        when (owner) {
            // lock when activity hits onStop and it isn’t a config-change restart
            is Activity -> {
                if (!owner.isChangingConfigurations) {
                    maybeLockPrivateModeOnStop()
                }
            }

            // lock when tabs fragment is getting closed in regular mode
            is TabsTrayFragment -> {
                maybeLockPrivateModeOnTabsTrayClosure()
            }
        }
    }

    private fun maybeLockPrivateModeOnStop() {
        // When the app gets inactive in private mode with opened tabs, we lock the private mode.
        val isPrivateModeLockEnabled = settings.privateBrowsingLockedEnabled
        val hasPrivateTabs = browserStore.state.privateTabs.isNotEmpty()
        val isPrivateMode = appStore.state.mode == BrowsingMode.Private

        if (isPrivateModeLockEnabled && isPrivateMode && hasPrivateTabs) {
            appStore.dispatch(
                PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                    isLocked = true,
                ),
            )
        }
    }

    private fun maybeLockPrivateModeOnTabsTrayClosure() {
        val isPrivateModeLockEnabled = settings.privateBrowsingLockedEnabled
        val hasPrivateTabs = browserStore.state.privateTabs.isNotEmpty()
        val isNormalMode = appStore.state.mode == BrowsingMode.Normal

        if (isPrivateModeLockEnabled && isNormalMode && hasPrivateTabs) {
            appStore.dispatch(
                PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                    isLocked = true,
                ),
            )
        }
    }
}

/**
 * Observes the app state and triggers a callback when the user enters a locked private browsing session.
 *
 * The observer is active in the [Lifecycle.State.RESUMED] state to ensure the UI state is sync with the app state (the
 * app state might be updated while the UI is no longer active.
 *
 * @param viewLifecycleOwner The [LifecycleOwner] used to control when the observation is active.
 * @param scope The [CoroutineScope] in which the coroutine will be launched.
 * @param appStore The [AppStore] to observe the [AppState].
 * @param onPrivateModeLocked A callback invoked when private browsing mode is locked.
 */
fun observePrivateModeLock(
    viewLifecycleOwner: LifecycleOwner,
    scope: CoroutineScope,
    appStore: AppStore,
    onPrivateModeLocked: () -> Unit,
) {
    with(viewLifecycleOwner) {
        scope.launch {
            lifecycle.repeatOnLifecycle(RESUMED) {
                appStore.flow()
                    .filter { state ->
                        state.isPrivateScreenLocked && state.mode == BrowsingMode.Private
                    }
                    .distinctUntilChanged()
                    .collect {
                        onPrivateModeLocked()
                    }
            }
        }
    }
}
