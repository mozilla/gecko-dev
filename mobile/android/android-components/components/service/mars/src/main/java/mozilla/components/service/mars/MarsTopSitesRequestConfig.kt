/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.mars

/**
 * Configuration for the top sites tile request.
 *
 * @property contextId An UUID that represents the user's context.
 * @property userAgent TODO
 * @property placements List of [Placement]s to request.
 */
data class MarsTopSitesRequestConfig(
    val contextId: String,
    val userAgent: String?,
    val placements: List<Placement>,
)

/**
 * An object representing the top sites tile to request.
 *
 * @property placement The ID of the top site tile placement to request.
 * @property count Number of top site tile placement to request.
 */
data class Placement(
    val placement: String,
    val count: Int,
)
