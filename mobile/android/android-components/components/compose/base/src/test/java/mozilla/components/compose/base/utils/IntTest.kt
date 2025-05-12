/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.compose.base.utils

import org.junit.Assert
import org.junit.Test
import java.util.Locale

class IntTest {

    @Test
    fun `WHEN the language is Arabic THEN translate the number to the proper symbol of that locale`() {
        val expected = "٥"
        val numberUnderTest = 5

        Locale.setDefault(Locale.forLanguageTag("ar"))

        Assert.assertEquals(expected, numberUnderTest.toLocaleString())
    }
}
