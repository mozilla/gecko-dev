/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.pocket

import mozilla.components.service.pocket.PocketStory

/**
 * Represents a [PocketStory] impression which contains the story shown and their respective
 * position.
 *
 * @property story The [PocketStory] that was shown.
 * @property position The position (0-index) of the [PocketStory].
 */
data class PocketImpression(
    val story: PocketStory,
    val position: Int,
)
