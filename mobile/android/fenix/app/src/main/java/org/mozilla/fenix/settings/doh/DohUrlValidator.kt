/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.doh

import java.net.URI
import java.net.URISyntaxException

/**
 * An object for DoH custom provider and exception urls.
 */
object DohUrlValidator {
    /**
     * Validates that the provided [url] is a valid absolute https url.
     *
     * @param url The URL to be validated.
     * @return The normalized url as a string.
     *
     * @throws UrlValidationException.NonHttpsUrlException If the url scheme is not "https".
     * @throws UrlValidationException.InvalidUrlException If the url is invalid or does not have a valid host.
     */
    @Suppress("ThrowsCount")
    fun validate(url: String): String {
        // This check is necessary because urls starting with "https:/" or "https:\"
        // would be considered "invalid" in java.net.URI
        if (!url.startsWith("https://")) {
            throw UrlValidationException.NonHttpsUrlException
        }
        return try {
            val uri = URI.create(url.trim())
            when {
                uri.scheme != "https" -> throw UrlValidationException.NonHttpsUrlException
                uri.host.isNullOrBlank() || !uri.isAbsolute -> throw UrlValidationException.InvalidUrlException
                else -> uri.toString()
            }
        } catch (e: URISyntaxException) {
            throw UrlValidationException.InvalidUrlException
        } catch (e: IllegalArgumentException) {
            throw UrlValidationException.InvalidUrlException
        }
    }

    /**
     * Removes the scheme (e.g. "https://", "http://") from the given [url].
     *
     * For instance:
     * - "http://example.com" -> "c.com"
     * - "https://foo"    -> "foo"
     * - "ftp://foo/bar"  -> "foo/bar"
     * - "example.com"    -> "example.com"
     *
     * @param url The original URL string.
     * @return The [url] without its leading scheme and "://", if present.
     */
    @Suppress("TooGenericExceptionCaught")
    fun dropScheme(url: String): String {
        // Only drop the scheme if the url is valid
        return try {
            val uri = URI.create(url)
            val prefix = "${uri.scheme}://"
            uri.toString().removePrefix(prefix)
        } catch (e: NullPointerException) {
            // a scheme does not exist
            url
        } catch (e: URISyntaxException) {
            url
        } catch (e: IllegalArgumentException) {
            url
        }
    }
}

/**
 * Represents the possible exceptions thrown during DoH url validation.
 */
sealed class UrlValidationException : RuntimeException() {
    /**
     * Indicates that the provided url does not use https as its scheme.
     */
    data object NonHttpsUrlException : UrlValidationException()

    /**
     * Indicates that the provided url is invalid (e.g. malformed, missing host).
     */
    data object InvalidUrlException : UrlValidationException()
}
