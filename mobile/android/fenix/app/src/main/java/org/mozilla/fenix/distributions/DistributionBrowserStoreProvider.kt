/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.distributions

import mozilla.components.browser.state.action.UpdateDistribution
import mozilla.components.browser.state.store.BrowserStore

/**
 * A tool for getting and updating distribution Ids from the browser store.
 *
 * The distribution Id is the Id used to tell which distribution deal the app install was involved
 * with.
 */
interface DistributionBrowserStoreProvider {
    /**
     * @return stored distribution Id
     */
    fun getDistributionId(): String?

    /**
     * updates the distribution Id in the browser store
     *
     * @param id the distribution Id to store
     */
    fun updateDistributionId(id: String)
}

/**
 * Default implementation for [DistributionBrowserStoreProvider]
 */
class DefaultDistributionBrowserStoreProvider(
    private val browserStore: BrowserStore,
) : DistributionBrowserStoreProvider {
    override fun getDistributionId(): String? = browserStore.state.distributionId

    override fun updateDistributionId(id: String) {
        browserStore.dispatch(UpdateDistribution(id))
    }
}
