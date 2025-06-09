/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.fake

import mozilla.components.feature.downloads.DateTimeProvider

class FakeDateTimeProvider(
    private val currentTime: Long = 0,
) : DateTimeProvider {

    override fun currentTimeMillis(): Long = currentTime
}
