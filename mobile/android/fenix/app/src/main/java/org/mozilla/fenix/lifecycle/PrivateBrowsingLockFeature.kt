/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.lifecycle

import android.app.Activity
import androidx.lifecycle.DefaultLifecycleObserver
import androidx.lifecycle.LifecycleOwner
import androidx.navigation.NavController
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.lib.state.ext.flowScoped
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.appstate.AppAction.PrivateBrowsingLockAction

/**
 * A lifecycle-aware feature that locks private browsing mode behind authentication
 * when certain conditions are met (e.g., switching modes or backgrounding the app).
 */
class PrivateBrowsingLockFeature(
    private val components: Components,
) : DefaultLifecycleObserver {

    init {
        // When the app is initialized, if there are private tabs, we should lock the private mode.
        // NB: This might need a check if the feature is enabled. As part of future work, we
        // should make sure that lock state resets when the user enables/disables the feature.
        components.appStore.dispatch(
            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                isLocked = components.core.store.state.privateTabs.isNotEmpty(),
            ),
        )
    }

    override fun onStart(owner: LifecycleOwner) {
        components.core.store.flowScoped(owner) { flow ->
            flow
                .map { it.privateTabs.size }
                .distinctUntilChanged()
                .filter { it == 0 }
                .collect {
                    if (components.settings.privateBrowsingLockedEnabled) {
                        // When all private tabs are closed, we don't need to lock the private mode.
                        components.appStore.dispatch(
                            PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                                isLocked = false,
                            ),
                        )
                    }
                }
        }

        components.appStore.flowScoped(owner) { flow ->
            flow
                .map { it.mode }
                .distinctUntilChanged()
                .filter { it == BrowsingMode.Normal }
                .collect {
                    // When witching from private to normal mode with private tabs open,
                    // we lock the private mode.
                    val isPrivateModeLockEnabled = components.settings.privateBrowsingLockedEnabled
                    val hasPrivateTabs = components.core.store.state.privateTabs.isNotEmpty()

                    if (isPrivateModeLockEnabled && hasPrivateTabs) {
                        components.appStore.dispatch(
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

        // only lock when this isn’t a config-change restart
        if (owner !is Activity || !owner.isChangingConfigurations) {
            maybeLockPrivateModeOnStop()
        }
    }

    private fun maybeLockPrivateModeOnStop() {
        // When the app gets inactive in private mode with opened tabs, we lock the private mode.
        val isPrivateModeLockEnabled = components.settings.privateBrowsingLockedEnabled
        val hasPrivateTabs = components.core.store.state.privateTabs.isNotEmpty()
        val isPrivateMode = components.appStore.state.mode == BrowsingMode.Private

        if (isPrivateModeLockEnabled && isPrivateMode && hasPrivateTabs) {
            components.appStore.dispatch(
                PrivateBrowsingLockAction.UpdatePrivateBrowsingLock(
                    isLocked = true,
                ),
            )
        }
    }

    /**
     * Navigates to the unlock screen for private tabs if the current app state requires it.
     *
     * This should be called when entering or returning to a private browsing context
     * to ensure the user is authenticated before accessing private tabs.
     *
     * The screen is shown if:
     * - The app is in private browsing mode
     * - There are private tabs open
     * - The lock feature is enabled
     * - The private screen is currently locked
     *
     * @param navController The [NavController] used to navigate to the unlock screen.
     */
    fun maybeLockScreen(navController: NavController) {
        if (shouldShowUnlockScreen()) {
            navController.navigate(R.id.unlockPrivateTabsFragment)
        }
    }

    /**
     * We verify if all conditions are met to display the unlock private mode screen
     */
    private fun shouldShowUnlockScreen(): Boolean {
        val hasPrivateTabs = components.core.store.state.privateTabs.isNotEmpty()
        val biometricLockEnabled = components.settings.privateBrowsingLockedEnabled
        val isPrivateMode = components.appStore.state.mode == BrowsingMode.Private
        val isPrivateScreenLocked = components.appStore.state.isPrivateScreenLocked

        return isPrivateMode && hasPrivateTabs && biometricLockEnabled && isPrivateScreenLocked
    }
}
