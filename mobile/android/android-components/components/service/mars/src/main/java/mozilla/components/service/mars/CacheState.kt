/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.mars

/**
 * Current state of the cache.
 *
 * @param isCacheValid Whether or not the current set of cached top sites is still valid.
 * @param localCacheMaxAge Maximum unix timestamp until the current set of cached top sites
 * is still valid, specified by the client.
 * @param serverCacheMaxAge Maximum unix timestamp until the current set of cached top sites
 * is still valid, specified by the server.
 */
internal data class CacheState(
    val isCacheValid: Boolean = true,
    val localCacheMaxAge: Long? = null,
    val serverCacheMaxAge: Long? = null,
) {
    fun getCacheMaxAge(shouldUseServerMaxAge: Boolean = false) = if (isCacheValid) {
        if (shouldUseServerMaxAge) serverCacheMaxAge else localCacheMaxAge
    } else {
        null
    }

    fun invalidate(): CacheState =
        this.copy(isCacheValid = false, localCacheMaxAge = null, serverCacheMaxAge = null)

    /**
     * Update local and server max age values.
     *
     * @param lastModified Unix timestamp when the cache was last modified.
     * @param localMaxAge Validity of local cache in milliseconds.
     * @param serverMaxAge Server specified validity in milliseconds. To be used as fallback
     * when local max age is exceeded and a server outage is detected.
     */
    fun computeMaxAges(lastModified: Long, localMaxAge: Long, serverMaxAge: Long): CacheState =
        this.copy(
            isCacheValid = true,
            localCacheMaxAge = lastModified + localMaxAge,
            serverCacheMaxAge = lastModified + serverMaxAge,
        )
}
