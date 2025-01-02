/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.contile

import mozilla.components.feature.top.sites.TopSite

/**
 * Data stored in the cache file
 *
 * @param validFor Time in milliseconds describing the click validity for the set of top sites.
 * @param topSites List of provided top sites.
 */
internal data class CachedData(
    val validFor: Long,
    val topSites: List<TopSite.Provided>,
)
