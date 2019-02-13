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
package ch.boye.httpclientandroidlib.impl.cookie;

import java.util.Locale;

import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.cookie.Cookie;
import ch.boye.httpclientandroidlib.cookie.CookieAttributeHandler;
import ch.boye.httpclientandroidlib.cookie.CookieOrigin;
import ch.boye.httpclientandroidlib.cookie.CookieRestrictionViolationException;
import ch.boye.httpclientandroidlib.cookie.MalformedCookieException;
import ch.boye.httpclientandroidlib.cookie.SetCookie;
import ch.boye.httpclientandroidlib.util.Args;

/**
 *
 * @since 4.0
 */
@Immutable
public class RFC2109DomainHandler implements CookieAttributeHandler {

    public RFC2109DomainHandler() {
        super();
    }

    public void parse(final SetCookie cookie, final String value)
            throws MalformedCookieException {
        Args.notNull(cookie, "Cookie");
        if (value == null) {
            throw new MalformedCookieException("Missing value for domain attribute");
        }
        if (value.trim().length() == 0) {
            throw new MalformedCookieException("Blank value for domain attribute");
        }
        cookie.setDomain(value);
    }

    public void validate(final Cookie cookie, final CookieOrigin origin)
            throws MalformedCookieException {
        Args.notNull(cookie, "Cookie");
        Args.notNull(origin, "Cookie origin");
        String host = origin.getHost();
        final String domain = cookie.getDomain();
        if (domain == null) {
            throw new CookieRestrictionViolationException("Cookie domain may not be null");
        }
        if (!domain.equals(host)) {
            int dotIndex = domain.indexOf('.');
            if (dotIndex == -1) {
                throw new CookieRestrictionViolationException("Domain attribute \""
                        + domain
                        + "\" does not match the host \""
                        + host + "\"");
            }
            // domain must start with dot
            if (!domain.startsWith(".")) {
                throw new CookieRestrictionViolationException("Domain attribute \""
                    + domain
                    + "\" violates RFC 2109: domain must start with a dot");
            }
            // domain must have at least one embedded dot
            dotIndex = domain.indexOf('.', 1);
            if (dotIndex < 0 || dotIndex == domain.length() - 1) {
                throw new CookieRestrictionViolationException("Domain attribute \""
                    + domain
                    + "\" violates RFC 2109: domain must contain an embedded dot");
            }
            host = host.toLowerCase(Locale.ENGLISH);
            if (!host.endsWith(domain)) {
                throw new CookieRestrictionViolationException(
                    "Illegal domain attribute \"" + domain
                    + "\". Domain of origin: \"" + host + "\"");
            }
            // host minus domain may not contain any dots
            final String hostWithoutDomain = host.substring(0, host.length() - domain.length());
            if (hostWithoutDomain.indexOf('.') != -1) {
                throw new CookieRestrictionViolationException("Domain attribute \""
                    + domain
                    + "\" violates RFC 2109: host minus domain may not contain any dots");
            }
        }
    }

    public boolean match(final Cookie cookie, final CookieOrigin origin) {
        Args.notNull(cookie, "Cookie");
        Args.notNull(origin, "Cookie origin");
        final String host = origin.getHost();
        final String domain = cookie.getDomain();
        if (domain == null) {
            return false;
        }
        return host.equals(domain) || (domain.startsWith(".") && host.endsWith(domain));
    }

}
