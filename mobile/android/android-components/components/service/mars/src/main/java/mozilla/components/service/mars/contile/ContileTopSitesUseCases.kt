/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.mars.contile

import androidx.annotation.VisibleForTesting
import mozilla.components.feature.top.sites.TopSitesProvider

/**
 * Contains use cases related to the Contlie top sites feature.
 */
internal class ContileTopSitesUseCases {

    /**
     * Refresh Contile top sites use case.
     */
    class RefreshContileTopSitesUseCase internal constructor() {
        /**
         * Refreshes the Contile top sites.
         */
        suspend operator fun invoke() {
            requireContileTopSitesProvider().getTopSites(allowCache = false)
        }
    }

    internal companion object {
        @VisibleForTesting internal var provider: TopSitesProvider? = null

        /**
         * Initializes the [TopSitesProvider] which will fetch the top sites tile from the
         * provider.
         */
        internal fun initialize(provider: TopSitesProvider) {
            this.provider = provider
        }

        /**
         * Unbinds the [TopSitesProvider].
         */
        internal fun destroy() {
            this.provider = null
        }

        /**
         * Returns the [TopSitesProvider], otherwise throw an exception if the [provider]
         * has not been initialized.
         */
        internal fun requireContileTopSitesProvider(): TopSitesProvider {
            return requireNotNull(provider) {
                "initialize must be called before trying to access the TopSitesProvider"
            }
        }
    }

    val refreshContileTopSites: RefreshContileTopSitesUseCase by lazy {
        RefreshContileTopSitesUseCase()
    }
}
