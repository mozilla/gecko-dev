/*
 * ====================================================================
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Software Foundation.  For more
 * information on the Apache Software Foundation, please see
 * <http://www.apache.org/>.
 *
 */
package ch.boye.httpclientandroidlib.client.utils;

import java.net.URI;
import java.net.URISyntaxException;
import java.util.List;
import java.util.Locale;
import java.util.Stack;

import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.util.Args;
import ch.boye.httpclientandroidlib.util.TextUtils;

/**
 * A collection of utilities for {@link URI URIs}, to workaround
 * bugs within the class or for ease-of-use features.
 *
 * @since 4.0
 */
@Immutable
public class URIUtils {

     /**
         * Constructs a {@link URI} using all the parameters. This should be
         * used instead of
         * {@link URI#URI(String, String, String, int, String, String, String)}
         * or any of the other URI multi-argument URI constructors.
         *
         * @param scheme
         *            Scheme name
         * @param host
         *            Host name
         * @param port
         *            Port number
         * @param path
         *            Path
         * @param query
         *            Query
         * @param fragment
         *            Fragment
         *
         * @throws URISyntaxException
         *             If both a scheme and a path are given but the path is
         *             relative, if the URI string constructed from the given
         *             components violates RFC&nbsp;2396, or if the authority
         *             component of the string is present but cannot be parsed
         *             as a server-based authority
         *
         * @deprecated (4.2) use {@link URIBuilder}.
         */
    @Deprecated
    public static URI createURI(
            final String scheme,
            final String host,
            final int port,
            final String path,
            final String query,
            final String fragment) throws URISyntaxException {
        final StringBuilder buffer = new StringBuilder();
        if (host != null) {
            if (scheme != null) {
                buffer.append(scheme);
                buffer.append("://");
            }
            buffer.append(host);
            if (port > 0) {
                buffer.append(':');
                buffer.append(port);
            }
        }
        if (path == null || !path.startsWith("/")) {
            buffer.append('/');
        }
        if (path != null) {
            buffer.append(path);
        }
        if (query != null) {
            buffer.append('?');
            buffer.append(query);
        }
        if (fragment != null) {
            buffer.append('#');
            buffer.append(fragment);
        }
        return new URI(buffer.toString());
    }

    /**
     * A convenience method for creating a new {@link URI} whose scheme, host
     * and port are taken from the target host, but whose path, query and
     * fragment are taken from the existing URI. The fragment is only used if
     * dropFragment is false. The path is set to "/" if not explicitly specified.
     *
     * @param uri
     *            Contains the path, query and fragment to use.
     * @param target
     *            Contains the scheme, host and port to use.
     * @param dropFragment
     *            True if the fragment should not be copied.
     *
     * @throws URISyntaxException
     *             If the resulting URI is invalid.
     */
    public static URI rewriteURI(
            final URI uri,
            final HttpHost target,
            final boolean dropFragment) throws URISyntaxException {
        Args.notNull(uri, "URI");
        if (uri.isOpaque()) {
            return uri;
        }
        final URIBuilder uribuilder = new URIBuilder(uri);
        if (target != null) {
            uribuilder.setScheme(target.getSchemeName());
            uribuilder.setHost(target.getHostName());
            uribuilder.setPort(target.getPort());
        } else {
            uribuilder.setScheme(null);
            uribuilder.setHost(null);
            uribuilder.setPort(-1);
        }
        if (dropFragment) {
            uribuilder.setFragment(null);
        }
        if (TextUtils.isEmpty(uribuilder.getPath())) {
            uribuilder.setPath("/");
        }
        return uribuilder.build();
    }

    /**
     * A convenience method for
     * {@link URIUtils#rewriteURI(URI, HttpHost, boolean)} that always keeps the
     * fragment.
     */
    public static URI rewriteURI(
            final URI uri,
            final HttpHost target) throws URISyntaxException {
        return rewriteURI(uri, target, false);
    }

    /**
     * A convenience method that creates a new {@link URI} whose scheme, host, port, path,
     * query are taken from the existing URI, dropping any fragment or user-information.
     * The path is set to "/" if not explicitly specified. The existing URI is returned
     * unmodified if it has no fragment or user-information and has a path.
     *
     * @param uri
     *            original URI.
     * @throws URISyntaxException
     *             If the resulting URI is invalid.
     */
    public static URI rewriteURI(final URI uri) throws URISyntaxException {
        Args.notNull(uri, "URI");
        if (uri.isOpaque()) {
            return uri;
        }
        final URIBuilder uribuilder = new URIBuilder(uri);
        if (uribuilder.getUserInfo() != null) {
            uribuilder.setUserInfo(null);
        }
        if (TextUtils.isEmpty(uribuilder.getPath())) {
            uribuilder.setPath("/");
        }
        if (uribuilder.getHost() != null) {
            uribuilder.setHost(uribuilder.getHost().toLowerCase(Locale.ENGLISH));
        }
        uribuilder.setFragment(null);
        return uribuilder.build();
    }

    /**
     * Resolves a URI reference against a base URI. Work-around for bug in
     * java.net.URI (<http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=4708535>)
     *
     * @param baseURI the base URI
     * @param reference the URI reference
     * @return the resulting URI
     */
    public static URI resolve(final URI baseURI, final String reference) {
        return URIUtils.resolve(baseURI, URI.create(reference));
    }

    /**
     * Resolves a URI reference against a base URI. Work-around for bugs in
     * java.net.URI (e.g. <http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=4708535>)
     *
     * @param baseURI the base URI
     * @param reference the URI reference
     * @return the resulting URI
     */
    public static URI resolve(final URI baseURI, final URI reference){
        Args.notNull(baseURI, "Base URI");
        Args.notNull(reference, "Reference URI");
        URI ref = reference;
        final String s = ref.toString();
        if (s.startsWith("?")) {
            return resolveReferenceStartingWithQueryString(baseURI, ref);
        }
        final boolean emptyReference = s.length() == 0;
        if (emptyReference) {
            ref = URI.create("#");
        }
        URI resolved = baseURI.resolve(ref);
        if (emptyReference) {
            final String resolvedString = resolved.toString();
            resolved = URI.create(resolvedString.substring(0,
                resolvedString.indexOf('#')));
        }
        return normalizeSyntax(resolved);
    }

    /**
     * Resolves a reference starting with a query string.
     *
     * @param baseURI the base URI
     * @param reference the URI reference starting with a query string
     * @return the resulting URI
     */
    private static URI resolveReferenceStartingWithQueryString(
            final URI baseURI, final URI reference) {
        String baseUri = baseURI.toString();
        baseUri = baseUri.indexOf('?') > -1 ?
            baseUri.substring(0, baseUri.indexOf('?')) : baseUri;
        return URI.create(baseUri + reference.toString());
    }

    /**
     * Removes dot segments according to RFC 3986, section 5.2.4 and
     * Syntax-Based Normalization according to RFC 3986, section 6.2.2.
     *
     * @param uri the original URI
     * @return the URI without dot segments
     */
    private static URI normalizeSyntax(final URI uri) {
        if (uri.isOpaque() || uri.getAuthority() == null) {
            // opaque and file: URIs
            return uri;
        }
        Args.check(uri.isAbsolute(), "Base URI must be absolute");
        final String path = uri.getPath() == null ? "" : uri.getPath();
        final String[] inputSegments = path.split("/");
        final Stack<String> outputSegments = new Stack<String>();
        for (final String inputSegment : inputSegments) {
            if ((inputSegment.length() == 0)
                || (".".equals(inputSegment))) {
                // Do nothing
            } else if ("..".equals(inputSegment)) {
                if (!outputSegments.isEmpty()) {
                    outputSegments.pop();
                }
            } else {
                outputSegments.push(inputSegment);
            }
        }
        final StringBuilder outputBuffer = new StringBuilder();
        for (final String outputSegment : outputSegments) {
            outputBuffer.append('/').append(outputSegment);
        }
        if (path.lastIndexOf('/') == path.length() - 1) {
            // path.endsWith("/") || path.equals("")
            outputBuffer.append('/');
        }
        try {
            final String scheme = uri.getScheme().toLowerCase(Locale.ENGLISH);
            final String auth = uri.getAuthority().toLowerCase(Locale.ENGLISH);
            final URI ref = new URI(scheme, auth, outputBuffer.toString(),
                    null, null);
            if (uri.getQuery() == null && uri.getFragment() == null) {
                return ref;
            }
            final StringBuilder normalized = new StringBuilder(
                    ref.toASCIIString());
            if (uri.getQuery() != null) {
                // query string passed through unchanged
                normalized.append('?').append(uri.getRawQuery());
            }
            if (uri.getFragment() != null) {
                // fragment passed through unchanged
                normalized.append('#').append(uri.getRawFragment());
            }
            return URI.create(normalized.toString());
        } catch (final URISyntaxException e) {
            throw new IllegalArgumentException(e);
        }
    }

    /**
     * Extracts target host from the given {@link URI}.
     *
     * @param uri
     * @return the target host if the URI is absolute or <code>null</null> if the URI is
     * relative or does not contain a valid host name.
     *
     * @since 4.1
     */
    public static HttpHost extractHost(final URI uri) {
        if (uri == null) {
            return null;
        }
        HttpHost target = null;
        if (uri.isAbsolute()) {
            int port = uri.getPort(); // may be overridden later
            String host = uri.getHost();
            if (host == null) { // normal parse failed; let's do it ourselves
                // authority does not seem to care about the valid character-set for host names
                host = uri.getAuthority();
                if (host != null) {
                    // Strip off any leading user credentials
                    final int at = host.indexOf('@');
                    if (at >= 0) {
                        if (host.length() > at+1 ) {
                            host = host.substring(at+1);
                        } else {
                            host = null; // @ on its own
                        }
                    }
                    // Extract the port suffix, if present
                    if (host != null) {
                        final int colon = host.indexOf(':');
                        if (colon >= 0) {
                            final int pos = colon + 1;
                            int len = 0;
                            for (int i = pos; i < host.length(); i++) {
                                if (Character.isDigit(host.charAt(i))) {
                                    len++;
                                } else {
                                    break;
                                }
                            }
                            if (len > 0) {
                                try {
                                    port = Integer.parseInt(host.substring(pos, pos + len));
                                } catch (final NumberFormatException ex) {
                                }
                            }
                            host = host.substring(0, colon);
                        }
                    }
                }
            }
            final String scheme = uri.getScheme();
            if (!TextUtils.isBlank(host)) {
                target = new HttpHost(host, port, scheme);
            }
        }
        return target;
    }

    /**
     * Derives the interpreted (absolute) URI that was used to generate the last
     * request. This is done by extracting the request-uri and target origin for
     * the last request and scanning all the redirect locations for the last
     * fragment identifier, then combining the result into a {@link URI}.
     *
     * @param originalURI
     *            original request before any redirects
     * @param target
     *            if the last URI is relative, it is resolved against this target,
     *            or <code>null</code> if not available.
     * @param redirects
     *            collection of redirect locations since the original request
     *            or <code>null</code> if not available.
     * @return interpreted (absolute) URI
     */
    public static URI resolve(
            final URI originalURI,
            final HttpHost target,
            final List<URI> redirects) throws URISyntaxException {
        Args.notNull(originalURI, "Request URI");
        final URIBuilder uribuilder;
        if (redirects == null || redirects.isEmpty()) {
            uribuilder = new URIBuilder(originalURI);
        } else {
            uribuilder = new URIBuilder(redirects.get(redirects.size() - 1));
            String frag = uribuilder.getFragment();
            // read interpreted fragment identifier from redirect locations
            for (int i = redirects.size() - 1; frag == null && i >= 0; i--) {
                frag = redirects.get(i).getFragment();
            }
            uribuilder.setFragment(frag);
        }
        // read interpreted fragment identifier from original request
        if (uribuilder.getFragment() == null) {
            uribuilder.setFragment(originalURI.getFragment());
        }
        // last target origin
        if (target != null && !uribuilder.isAbsolute()) {
            uribuilder.setScheme(target.getSchemeName());
            uribuilder.setHost(target.getHostName());
            uribuilder.setPort(target.getPort());
        }
        return uribuilder.build();
    }

    /**
     * This class should not be instantiated.
     */
    private URIUtils() {
    }

}
