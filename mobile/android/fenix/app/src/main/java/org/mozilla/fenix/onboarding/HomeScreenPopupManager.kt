/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import mozilla.components.support.base.feature.LifecycleAwareFeature
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.utils.Settings

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
    private val settings: Settings,
    private val nimbusManager: HomeScreenPopupManagerNimbusManager = DefaultHomeScreenPopupManagerNimbusManager(),
) : LifecycleAwareFeature, SearchBarCFRVisibility {

    private var scope: CoroutineScope? = null

    private val _searchBarCRFVisibility = MutableStateFlow(false)
    override val searchBarCFRVisibility: StateFlow<Boolean> = _searchBarCRFVisibility.asStateFlow()

    override fun onSearchBarCFRDismissed() {
        settings.lastCfrShownTimeInMillis = System.currentTimeMillis()
        settings.shouldShowSearchBarCFR = false
        _searchBarCRFVisibility.value = false
    }

    override fun start() {
        if (settings.shouldShowSearchBarCFR && settings.canShowCfr) {
            nimbusManager.recordEncourageSearchCrfExposure()
            _searchBarCRFVisibility.value = true
        }
    }

    override fun stop() {
        scope?.cancel()
    }
}
