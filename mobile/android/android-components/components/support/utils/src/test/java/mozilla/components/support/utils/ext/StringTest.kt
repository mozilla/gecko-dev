/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.utils.ext

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class StringTest {

    @Test
    fun `GIVEN a string credit card number WHEN calling toCreditCardNumber THEN any character that is not a digit will removed`() {
        val number = "385  -  2 0 0 - 0 0 0 2 3 2 3   7"
        val expectedResult = "38520000023237"

        assertEquals(expectedResult, number.toCreditCardNumber())
    }

    @Test
    fun `GIVEN a content Url WHEN calling isContentUrl THEN returns true`() {
        val url = "content://contenturl"
        assertTrue(url.isContentUrl())
    }

    @Test
    fun `GIVEN an internet Url WHEN calling isContentUrl THEN returns false`() {
        val url = "https://mozilla.org"
        assertFalse(url.isContentUrl())
    }
}
