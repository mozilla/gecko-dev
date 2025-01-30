/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.pocket.mars.api

import kotlinx.serialization.Serializable

/**
 * Configuration for the sponsored contents (spocs) request.
 *
 * @property contextId An UUID that represents the user's context.
 * @property userAgent The user agent to be used for the request.
 * @property placements List of [Placement]s to request.
 */
data class MarsSpocsRequestConfig(
    val contextId: String,
    val userAgent: String?,
    val placements: List<Placement>,
)

/**
 * An object representing the sponsored contents to request.
 *
 * @property placement The ID of the sponsored content placement to request.
 * @property count Number of sponsored contents to request.
 */
@Serializable
data class Placement(
    val placement: String,
    val count: Int,
)
