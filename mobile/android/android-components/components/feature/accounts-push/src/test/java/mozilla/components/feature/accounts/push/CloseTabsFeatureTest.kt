/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.accounts.push

import mozilla.components.concept.sync.Device
import mozilla.components.concept.sync.DeviceCapability
import mozilla.components.concept.sync.DeviceCommandIncoming
import mozilla.components.concept.sync.DeviceType
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito.verify

class CloseTabsFeatureTest {
    private val receiver: CloseTabsCommandReceiver = mock()

    private val device123 = Device(
        id = "123",
        displayName = "Charcoal",
        deviceType = DeviceType.DESKTOP,
        isCurrentDevice = false,
        lastAccessTime = null,
        capabilities = listOf(DeviceCapability.CLOSE_TABS),
        subscriptionExpired = true,
        subscription = null,
    )

    @Test
    fun `WHEN the account events observer is notified THEN the close tabs command receiver is invoked`() {
        val urls = listOf(
            "https://mozilla.org",
            "https://getfirefox.com",
            "https://example.org",
            "https://getthunderbird.com",
        )
        val feature = CloseTabsFeature(
            receiver = receiver,
            accountManager = mock(),
            owner = mock(),
        )

        feature.observer.onTabsClosed(device123, urls)

        val incomingCommandCaptor = argumentCaptor<DeviceCommandIncoming.TabsClosed>()
        verify(receiver).receive(incomingCommandCaptor.capture())
        assertEquals(device123, incomingCommandCaptor.value.from)
        assertEquals(urls, incomingCommandCaptor.value.urls)
    }
}
