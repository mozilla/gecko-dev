/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import androidx.core.net.toUri
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.settings.SupportUtils
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class UriTest {
    @Test
    fun `WHEN urlContainsQueryParameters is invoked THEN the result should be true only if the url contains the search parameters`() {
        var searchParameters = ""
        val googleSite = SupportUtils.GOOGLE_URL.toUri()
        val querySite = "test.com/?q=value".toUri()
        val blankQuerySite = "test.com/?q=".toUri()

        assertFalse(googleSite.containsQueryParameters(searchParameters))
        assertFalse(querySite.containsQueryParameters(searchParameters))
        assertFalse(blankQuerySite.containsQueryParameters(searchParameters))

        searchParameters = "q"

        assertFalse(googleSite.containsQueryParameters(searchParameters))
        assertFalse(querySite.containsQueryParameters(searchParameters))
        assertTrue(blankQuerySite.containsQueryParameters(searchParameters))

        searchParameters = "q="

        assertFalse(googleSite.containsQueryParameters(searchParameters))
        assertFalse(querySite.containsQueryParameters(searchParameters))
        assertTrue(blankQuerySite.containsQueryParameters(searchParameters))

        searchParameters = "q=value"

        assertFalse(googleSite.containsQueryParameters(searchParameters))
        assertTrue(querySite.containsQueryParameters(searchParameters))
        assertFalse(blankQuerySite.containsQueryParameters(searchParameters))
    }

    @Test
    fun `WHEN an opaque url is checked for query parameters THEN then the result should be false`() {
        val searchParameters = "q"
        val opaqueUrl = "about:config".toUri()
        val mailToUrl = "mailto:a@b.com".toUri()

        assertFalse(opaqueUrl.containsQueryParameters(searchParameters))
        assertFalse(mailToUrl.containsQueryParameters(searchParameters))
    }
}
