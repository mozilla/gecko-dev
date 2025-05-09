/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lifecycle

import android.app.Activity
import android.content.SharedPreferences
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.Lifecycle.State.RESUMED
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.repeatOnLifecycle
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
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

/**
 * An interface to access and observe the enabled/disabled state of the Private Browsing Lock feature.
 */
interface PrivateBrowsingLockStorage {
    /**
     * Returns the current enabled state of the private browsing lock feature.
     */
    val isFeatureEnabled: Boolean

    /**
     * Registers a listener that is invoked whenever the enabled state changes.
     *
     * @param listener A lambda that receives the new boolean value when it changes.
     */
    fun addFeatureStateListener(listener: (Boolean) -> Unit)

    /**
     * Removes the previously registered listener.
     */
    fun removeFeatureStateListener()
}

/**
 * A default implementation of `PrivateBrowsingLockStorage`.
 *
 * @param preferences The [SharedPreferences] instance from which to read the preference value.
 * @param privateBrowsingLockPrefKey The key in [SharedPreferences] representing the feature flag.
 */
class DefaultPrivateBrowsingLockStorage(
    private val preferences: SharedPreferences,
    private val privateBrowsingLockPrefKey: String,
) : PrivateBrowsingLockStorage {
    private var listener: ((Boolean) -> Unit)? = null
    private val onFeatureStateChanged = SharedPreferences.OnSharedPreferenceChangeListener { prefs, key ->
        if (key == privateBrowsingLockPrefKey) {
            listener?.invoke(prefs.getBoolean(privateBrowsingLockPrefKey, false))
        }
    }

    override val isFeatureEnabled: Boolean
        get() = preferences.getBoolean(privateBrowsingLockPrefKey, false)

    override fun addFeatureStateListener(listener: (Boolean) -> Unit) {
        this.listener = listener
        preferences.registerOnSharedPreferenceChangeListener(onFeatureStateChanged)
    }

    override fun removeFeatureStateListener() {
        preferences.unregisterOnSharedPreferenceChangeListener(onFeatureStateChanged)
        listener = null
    }
}

/**
 * A lifecycle-aware feature that locks private browsing mode behind authentication
 * when certain conditions are met (e.g., switching modes or backgrounding the app).
 */
class PrivateBrowsingLockFeature(
    private val appStore: AppStore,
    private val browserStore: BrowserStore,
    private val storage: PrivateBrowsingLockStorage,
) : DefaultLifecycleObserver {
    private var browserStoreScope: CoroutineScope? = null
    private var appStoreScope: CoroutineScope? = null
    private var isFeatureEnabled = false

    init {
        isFeatureEnabled = storage.isFeatureEnabled

        updateFeatureState(
            isFeatureEnabled = isFeatureEnabled,
            isLocked = browserStore.state.privateTabs.isNotEmpty(),
        )

        observeFeatureStateUpdates()
    }

    /**
     * Handles a successful authentication event by unlocking the private browsing mode.
     *
     * This should be called by biometric or password authentication mechanisms (e.g., fingerprint,
     * face unlock, or PIN entry) once the user has successfully authenticated. It updates the app state
     * to reflect that private browsing tabs are now accessible.
     */
    fun onSuccessfulAuthentication() {
        appStore.dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(isLocked = false),
        )
    }

    private fun observeFeatureStateUpdates() {
        storage.addFeatureStateListener { isEnabled ->
            isFeatureEnabled = isEnabled

            updateFeatureState(
                isFeatureEnabled = isFeatureEnabled,
                isLocked = appStore.state.mode == BrowsingMode.Normal &&
                        browserStore.state.privateTabs.isNotEmpty(),
            )
        }
    }

    private fun updateFeatureState(
        isFeatureEnabled: Boolean,
        isLocked: Boolean,
    ) {
        if (isFeatureEnabled) {
            start(isLocked)
        } else {
            stop()
        }
    }

    private fun start(isLocked: Boolean) {
        observePrivateTabsClosure()
        observeSwitchingToNormalMode()

        appStore.dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = isLocked,
            ),
        )
    }

    private fun stop() {
        browserStoreScope?.cancel()
        appStoreScope?.cancel()
        storage.removeFeatureStateListener()

        browserStoreScope = null
        appStoreScope = null

        appStore.dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = false,
            ),
        )
    }

    private fun observePrivateTabsClosure() {
        browserStoreScope = browserStore.flowScoped { flow ->
            flow
                .map { it.privateTabs.size }
                .distinctUntilChanged()
                .filter { it == 0 }
                .collect {
                    // When all private tabs are closed, we don't need to lock the private mode.
                    appStore.dispatch(
                        PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                            isLocked = false,
                        ),
                    )
                }
        }
    }

    private fun observeSwitchingToNormalMode() {
        appStoreScope = appStore.flowScoped { flow ->
            flow
                .map { it.mode }
                .distinctUntilChanged()
                .filter { it == BrowsingMode.Normal }
                .collect {
                    // When witching from private to normal mode with private tabs open,
                    // we lock the private mode.
                    val hasPrivateTabs = browserStore.state.privateTabs.isNotEmpty()
                    if (hasPrivateTabs) {
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

        if (!isFeatureEnabled) return

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
        val hasPrivateTabs = browserStore.state.privateTabs.isNotEmpty()
        val isPrivateMode = appStore.state.mode == BrowsingMode.Private

        if (isPrivateMode && hasPrivateTabs) {
            appStore.dispatch(
                PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                    isLocked = true,
                ),
            )
        }
    }

    private fun maybeLockPrivateModeOnTabsTrayClosure() {
        val hasPrivateTabs = browserStore.state.privateTabs.isNotEmpty()
        val isNormalMode = appStore.state.mode == BrowsingMode.Normal

        if (isNormalMode && hasPrivateTabs) {
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
