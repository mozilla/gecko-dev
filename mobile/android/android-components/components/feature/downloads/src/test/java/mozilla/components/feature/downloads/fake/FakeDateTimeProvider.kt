/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads.fake

import mozilla.components.feature.downloads.DateTimeProvider

class FakeDateTimeProvider(
    private val startTime: Long,
    private val currentTime: Long,
) : DateTimeProvider {

    private var startTimeCalled = false

    override fun currentTimeMillis(): Long {
        return if (startTimeCalled) {
            currentTime
        } else {
            setStartTimeCalled()
            startTime
        }
    }

    private fun setStartTimeCalled() {
        startTimeCalled = true
    }
}
