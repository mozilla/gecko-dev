/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import android.net.Uri;
import android.text.TextUtils;

public class StringUtils {

    private static final String FILTER_URL_PREFIX = "filter://";

    /*
     * This method tries to guess if the given string could be a search query or URL,
     * and returns a previous result if there is ambiguity
     *
     * Search examples:
     *  foo
     *  foo bar.com
     *  foo http://bar.com
     *
     * URL examples
     *  foo.com
     *  foo.c
     *  :foo
     *  http://foo.com bar
     *
     * wasSearchQuery specifies whether text was a search query before the latest change
     * in text. In ambiguous cases where the new text can be either a search or a URL,
     * wasSearchQuery is returned
    */
    public static boolean isSearchQuery(String text, boolean wasSearchQuery) {
        // We remove leading and trailing white spaces when decoding URLs
        text = text.trim();
        if (text.length() == 0)
            return wasSearchQuery;

        int colon = text.indexOf(':');
        int dot = text.indexOf('.');
        int space = text.indexOf(' ');

        // If a space is found before any dot and colon, we assume this is a search query
        if (space > -1 && (colon == -1 || space < colon) && (dot == -1 || space < dot)) {
            return true;
        }
        // Otherwise, if a dot or a colon is found, we assume this is a URL
        if (dot > -1 || colon > -1) {
            return false;
        }
        // Otherwise, text is ambiguous, and we keep its status unchanged
        return wasSearchQuery;
    }

    public static class UrlFlags {
        public static final int NONE = 0;
        public static final int STRIP_HTTPS = 1;
    }

    public static String stripScheme(String url) {
        return stripScheme(url, UrlFlags.NONE);
    }

    public static String stripScheme(String url, int flags) {
        if (url == null) {
            return url;
        }

        int start = 0;
        int end = url.length();

        if (url.startsWith("http://")) {
            start = 7;
        } else if (url.startsWith("https://") && flags == UrlFlags.STRIP_HTTPS) {
            start = 8;
        }

        if (url.endsWith("/")) {
            end--;
        }

        return url.substring(start, end);
    }

    public static String stripCommonSubdomains(String host) {
        if (host == null) {
            return host;
        }

        // In contrast to desktop, we also strip mobile subdomains,
        // since its unlikely users are intentionally typing them
        int start = 0;

        if (host.startsWith("www.")) {
            start = 4;
        } else if (host.startsWith("mobile.")) {
            start = 7;
        } else if (host.startsWith("m.")) {
            start = 2;
        }

        return host.substring(start);
    }

    /**
     * Searches the url query string for the first value with the given key.
     */
    public static String getQueryParameter(String url, String desiredKey) {
        if (TextUtils.isEmpty(url) || TextUtils.isEmpty(desiredKey)) {
            return null;
        }

        final String[] urlParts = url.split("\\?");
        if (urlParts.length < 2) {
            return null;
        }

        final String query = urlParts[1];
        for (final String param : query.split("&")) {
            final String pair[] = param.split("=");
            final String key = Uri.decode(pair[0]);

            // Key is empty or does not match the key we're looking for, discard
            if (TextUtils.isEmpty(key) || !key.equals(desiredKey)) {
                continue;
            }
            // No value associated with key, discard
            if (pair.length < 2) {
                continue;
            }
            final String value = Uri.decode(pair[1]);
            if (TextUtils.isEmpty(value)) {
                return null;
            }
            return value;
        }

        return null;
    }

    public static boolean isFilterUrl(String url) {
        if (TextUtils.isEmpty(url)) {
            return false;
        }

        return url.startsWith(FILTER_URL_PREFIX);
    }

    public static String getFilterFromUrl(String url) {
        if (TextUtils.isEmpty(url)) {
            return null;
        }

        return url.substring(FILTER_URL_PREFIX.length());
    }

    public static boolean isShareableUrl(final String url) {
        final String scheme = Uri.parse(url).getScheme();
        return !("about".equals(scheme) || "chrome".equals(scheme) ||
                "file".equals(scheme) || "resource".equals(scheme));
    }
}
