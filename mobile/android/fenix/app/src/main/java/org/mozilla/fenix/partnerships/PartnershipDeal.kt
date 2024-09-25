/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.partnerships

/**
 * This enum represents partnership deal IDs that are used in glean metrics.
 */
enum class PartnershipDeal(
    val id: String,
) {
    VIVO_CPA_2024(
        id = "vivo-cpa-2024",
    ),
}
