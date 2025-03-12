/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import org.junit.Assert.assertEquals
import org.junit.Assert.assertThrows
import org.junit.Test

class DohUrlValidatorTest {
    @Test
    fun `WHEN a valid url is passed THEN the validator should return itself without any exception`() {
        assertEquals(
            DohUrlValidator.validate("https://example.com"),
            "https://example.com",
        )
    }

    @Test
    fun `WHEN a non-https url is passed THEN the validator should throw NonHttpsUrl exception`() {
        assertThrows(
            UrlValidationException.NonHttpsUrlException::class.java,
        ) {
            DohUrlValidator.validate("http://example.com")
        }
    }

    @Test
    fun `WHEN url does not start with a scheme followed by colon and two backslashes THEN the validator should throw NonHttpsUrl exception`() {
        // This is intended, unfortunately due to the nature of java.utils.URI
        assertThrows(
            UrlValidationException.NonHttpsUrlException::class.java,
        ) {
            DohUrlValidator.validate("https:/example.com")
        }
    }

    @Test
    fun `WHEN url has a blank host THEN the validator should throw InvalidUrl exception`() {
        assertThrows(
            UrlValidationException.InvalidUrlException::class.java,
        ) {
            DohUrlValidator.validate("https://?q=abc")
        }
    }

    @Test
    fun `WHEN url contains an invalid character THEN the validator should throw InvalidUrl exception`() {
        assertThrows(
            UrlValidationException.InvalidUrlException::class.java,
        ) {
            DohUrlValidator.validate("https://e@.com")
        }
    }

    @Test
    fun `WHEN url contains trailing spaces THEN the validator should simply drop them`() {
        assertEquals(
            DohUrlValidator.validate("https://example.com  "),
            "https://example.com",
        )
    }

    @Test
    fun `WHEN dropScheme is called with a url already without a scheme THEN no change should occur`() {
        val url = "foo.bar"
        assertEquals(
            DohUrlValidator.dropScheme(url),
            url,
        )
    }

    @Test
    fun `WHEN dropScheme is called with a valid url with a scheme THEN the scheme should be dropped`() {
        val url = "https://foo.bar"
        assertEquals(
            DohUrlValidator.dropScheme(url),
            "foo.bar",
        )
    }

    @Test
    fun `WHEN dropScheme is called with an invalid url THEN no change should occur`() {
        val url = "totally&!@#invalid://host"
        assertEquals(
            DohUrlValidator.dropScheme(url),
            url,
        )
    }
}
