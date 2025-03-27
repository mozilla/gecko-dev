/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.telemetry

import androidx.core.net.toUri
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class UtilsKtTest {
    @Test
    fun `GIVEN an Uri with uppercase for parameter keys WHEN lowercasing these THEN get the expected result`() {
        val uri = "https://mozilla.com/search?q=Firefox&ThIs=Test&AgaiN=TEST".toUri()
        val expected = "https://mozilla.com/search?q=Firefox&this=Test&again=TEST".toUri()

        val result = uri.lowercaseQueryParameterKeys()

        assertEquals(expected, result)
    }
}
