/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.utils.Settings

/**
 * Interface for the Navigation bar CFR Visibility.
 */
interface NavBarCFRVisibility {
    /**
     * Returns the StateFlow for navbar CFR visibility.
     */
    val navBarCFRVisibility: StateFlow<Boolean>

    /**
     * Sets navbar CFR visibility.
     */
    fun setNavbarCFRShown(cfrShown: Boolean)
}

/**
 * Delegate for handling CFR Visibility.
 */
class HomeScreenPopupManager(
    val appStore: AppStore,
    val settings: Settings,
) : LifecycleAwareFeature, NavBarCFRVisibility {
    private var isCFRAnchorVisible = false
    private var isNavigationBarEnabled = settings.shouldShowNavigationBarCFR
    private val showNavBarCFR = MutableStateFlow(false)

    private var scope: CoroutineScope? = null

    override val navBarCFRVisibility: StateFlow<Boolean>
        get() = showNavBarCFR.asStateFlow()

    override fun setNavbarCFRShown(cfrShown: Boolean) {
        isNavigationBarEnabled = !cfrShown
        settings.shouldShowNavigationBarCFR = !cfrShown
    }

    override fun start() {
        if (isNavigationBarEnabled) {
            scope = appStore.flowScoped { flow ->
                flow.distinctUntilChangedBy { it.isSearchDialogVisible }
                    .collect { state ->
                        isCFRAnchorVisible = !state.isSearchDialogVisible
                        showNavBarCFR.value = isCFRAnchorVisible && isNavigationBarEnabled
                    }
            }
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}
