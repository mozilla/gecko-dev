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
import org.mozilla.fenix.nimbus.FxNimbus
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
 * Interface for the Search bar CFR Visibility.
 */
interface SearchBarCFRVisibility {
    /**
     * Returns the StateFlow for the search bar CFR visibility
     */
    val searchBarCFRVisibility: StateFlow<Boolean>

    /**
     * Callback for when search bar CFR is dismissed
     */
    fun onSearchBarCFRDismissed()
}

/**
 * Interface for interacting with nimbus
 */
interface HomeScreenPopupManagerNimbusManager {
    /**
     * Records the encourage search CFR exposure
     */
    fun recordEncourageSearchCrfExposure()
}

/**
 * Default implementation for [HomeScreenPopupManagerNimbusManager]
 */
class DefaultHomeScreenPopupManagerNimbusManager : HomeScreenPopupManagerNimbusManager {
    override fun recordEncourageSearchCrfExposure() {
        FxNimbus.features.encourageSearchCfr.recordExposure()
    }
}

/**
 * Delegate for handling CFR Visibility.
 */
class HomeScreenPopupManager(
    private val appStore: AppStore,
    private val settings: Settings,
    private val nimbusManager: HomeScreenPopupManagerNimbusManager = DefaultHomeScreenPopupManagerNimbusManager(),
) : LifecycleAwareFeature, NavBarCFRVisibility, SearchBarCFRVisibility {
    private var isCFRAnchorVisible = false
    private var isNavigationBarEnabled = settings.shouldShowNavigationBarCFR
    private val showNavBarCFR = MutableStateFlow(false)

    private var scope: CoroutineScope? = null

    override val navBarCFRVisibility: StateFlow<Boolean>
        get() = showNavBarCFR.asStateFlow()

    private val _searchBarCRFVisibility = MutableStateFlow(false)
    override val searchBarCFRVisibility: StateFlow<Boolean> = _searchBarCRFVisibility.asStateFlow()

    override fun setNavbarCFRShown(cfrShown: Boolean) {
        if (!cfrShown) {
            settings.lastCfrShownTimeInMillis = System.currentTimeMillis()
        }
        isNavigationBarEnabled = !cfrShown
        settings.shouldShowNavigationBarCFR = !cfrShown
    }

    override fun onSearchBarCFRDismissed() {
        settings.lastCfrShownTimeInMillis = System.currentTimeMillis()
        settings.shouldShowSearchBarCFR = false
        _searchBarCRFVisibility.value = false
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
        } else if (settings.shouldShowSearchBarCFR && settings.canShowCfr) {
            nimbusManager.recordEncourageSearchCrfExposure()
            _searchBarCRFVisibility.value = true
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}
