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

package ch.boye.httpclientandroidlib.client.config;

import java.net.InetAddress;
import java.util.Collection;

import ch.boye.httpclientandroidlib.HttpHost;

public class RequestConfig implements Cloneable {

    public static final RequestConfig DEFAULT = new Builder().build();

    private final boolean expectContinueEnabled;
    private final HttpHost proxy;
    private final InetAddress localAddress;
    private final boolean staleConnectionCheckEnabled;
    private final String cookieSpec;
    private final boolean redirectsEnabled;
    private final boolean relativeRedirectsAllowed;
    private final boolean circularRedirectsAllowed;
    private final int maxRedirects;
    private final boolean authenticationEnabled;
    private final Collection<String> targetPreferredAuthSchemes;
    private final Collection<String> proxyPreferredAuthSchemes;
    private final int connectionRequestTimeout;
    private final int connectTimeout;
    private final int socketTimeout;

    RequestConfig(
            final boolean expectContinueEnabled,
            final HttpHost proxy,
            final InetAddress localAddress,
            final boolean staleConnectionCheckEnabled,
            final String cookieSpec,
            final boolean redirectsEnabled,
            final boolean relativeRedirectsAllowed,
            final boolean circularRedirectsAllowed,
            final int maxRedirects,
            final boolean authenticationEnabled,
            final Collection<String> targetPreferredAuthSchemes,
            final Collection<String> proxyPreferredAuthSchemes,
            final int connectionRequestTimeout,
            final int connectTimeout,
            final int socketTimeout) {
        super();
        this.expectContinueEnabled = expectContinueEnabled;
        this.proxy = proxy;
        this.localAddress = localAddress;
        this.staleConnectionCheckEnabled = staleConnectionCheckEnabled;
        this.cookieSpec = cookieSpec;
        this.redirectsEnabled = redirectsEnabled;
        this.relativeRedirectsAllowed = relativeRedirectsAllowed;
        this.circularRedirectsAllowed = circularRedirectsAllowed;
        this.maxRedirects = maxRedirects;
        this.authenticationEnabled = authenticationEnabled;
        this.targetPreferredAuthSchemes = targetPreferredAuthSchemes;
        this.proxyPreferredAuthSchemes = proxyPreferredAuthSchemes;
        this.connectionRequestTimeout = connectionRequestTimeout;
        this.connectTimeout = connectTimeout;
        this.socketTimeout = socketTimeout;
    }

    /**
     * Determines whether the 'Expect: 100-Continue' handshake is enabled
     * for entity enclosing methods. The purpose of the 'Expect: 100-Continue'
     * handshake is to allow a client that is sending a request message with
     * a request body to determine if the origin server is willing to
     * accept the request (based on the request headers) before the client
     * sends the request body.
     * <p/>
     * The use of the 'Expect: 100-continue' handshake can result in
     * a noticeable performance improvement for entity enclosing requests
     * (such as POST and PUT) that require the target server's
     * authentication.
     * <p/>
     * 'Expect: 100-continue' handshake should be used with caution, as it
     * may cause problems with HTTP servers and proxies that do not support
     * HTTP/1.1 protocol.
     * <p/>
     * Default: <code>false</code>
     */
    public boolean isExpectContinueEnabled() {
        return expectContinueEnabled;
    }

    /**
     * Returns HTTP proxy to be used for request execution.
     * <p/>
     * Default: <code>null</code>
     */
    public HttpHost getProxy() {
        return proxy;
    }

    /**
     * Returns local address to be used for request execution.
     * <p/>
     * On machines with multiple network interfaces, this parameter
     * can be used to select the network interface from which the
     * connection originates.
     * <p/>
     * Default: <code>null</code>
     */
    public InetAddress getLocalAddress() {
        return localAddress;
    }

    /**
     * Determines whether stale connection check is to be used. The stale
     * connection check can cause up to 30 millisecond overhead per request and
     * should be used only when appropriate. For performance critical
     * operations this check should be disabled.
     * <p/>
     * Default: <code>true</code>
     */
    public boolean isStaleConnectionCheckEnabled() {
        return staleConnectionCheckEnabled;
    }

    /**
     * Determines the name of the cookie specification to be used for HTTP state
     * management.
     * <p/>
     * Default: <code>null</code>
     */
    public String getCookieSpec() {
        return cookieSpec;
    }

    /**
     * Determines whether redirects should be handled automatically.
     * <p/>
     * Default: <code>true</code>
     */
    public boolean isRedirectsEnabled() {
        return redirectsEnabled;
    }

    /**
     * Determines whether relative redirects should be rejected. HTTP specification
     * requires the location value be an absolute URI.
     * <p/>
     * Default: <code>true</code>
     */
    public boolean isRelativeRedirectsAllowed() {
        return relativeRedirectsAllowed;
    }

    /**
     * Determines whether circular redirects (redirects to the same location) should
     * be allowed. The HTTP spec is not sufficiently clear whether circular redirects
     * are permitted, therefore optionally they can be enabled
     * <p/>
     * Default: <code>false</code>
     */
    public boolean isCircularRedirectsAllowed() {
        return circularRedirectsAllowed;
    }

    /**
     * Returns the maximum number of redirects to be followed. The limit on number
     * of redirects is intended to prevent infinite loops.
     * <p/>
     * Default: <code>50</code>
     */
    public int getMaxRedirects() {
        return maxRedirects;
    }

    /**
     * Determines whether authentication should be handled automatically.
     * <p/>
     * Default: <code>true</code>
     */
    public boolean isAuthenticationEnabled() {
        return authenticationEnabled;
    }

    /**
     * Determines the order of preference for supported authentication schemes
     * when authenticating with the target host.
     * <p/>
     * Default: <code>null</code>
     */
    public Collection<String> getTargetPreferredAuthSchemes() {
        return targetPreferredAuthSchemes;
    }

    /**
     * Determines the order of preference for supported authentication schemes
     * when authenticating with the proxy host.
     * <p/>
     * Default: <code>null</code>
     */
    public Collection<String> getProxyPreferredAuthSchemes() {
        return proxyPreferredAuthSchemes;
    }

    /**
     * Returns the timeout in milliseconds used when requesting a connection
     * from the connection manager. A timeout value of zero is interpreted
     * as an infinite timeout.
     * <p/>
     * A timeout value of zero is interpreted as an infinite timeout.
     * A negative value is interpreted as undefined (system default).
     * <p/>
     * Default: <code>-1</code>
     */
    public int getConnectionRequestTimeout() {
        return connectionRequestTimeout;
    }

    /**
     * Determines the timeout in milliseconds until a connection is established.
     * A timeout value of zero is interpreted as an infinite timeout.
     * <p/>
     * A timeout value of zero is interpreted as an infinite timeout.
     * A negative value is interpreted as undefined (system default).
     * <p/>
     * Default: <code>-1</code>
     */
    public int getConnectTimeout() {
        return connectTimeout;
    }

    /**
     * Defines the socket timeout (<code>SO_TIMEOUT</code>) in milliseconds,
     * which is the timeout for waiting for data  or, put differently,
     * a maximum period inactivity between two consecutive data packets).
     * <p/>
     * A timeout value of zero is interpreted as an infinite timeout.
     * A negative value is interpreted as undefined (system default).
     * <p/>
     * Default: <code>-1</code>
     */
    public int getSocketTimeout() {
        return socketTimeout;
    }

    @Override
    protected RequestConfig clone() throws CloneNotSupportedException {
        return (RequestConfig) super.clone();
    }

    @Override
    public String toString() {
        final StringBuilder builder = new StringBuilder();
        builder.append(", expectContinueEnabled=").append(expectContinueEnabled);
        builder.append(", proxy=").append(proxy);
        builder.append(", localAddress=").append(localAddress);
        builder.append(", staleConnectionCheckEnabled=").append(staleConnectionCheckEnabled);
        builder.append(", cookieSpec=").append(cookieSpec);
        builder.append(", redirectsEnabled=").append(redirectsEnabled);
        builder.append(", relativeRedirectsAllowed=").append(relativeRedirectsAllowed);
        builder.append(", maxRedirects=").append(maxRedirects);
        builder.append(", circularRedirectsAllowed=").append(circularRedirectsAllowed);
        builder.append(", authenticationEnabled=").append(authenticationEnabled);
        builder.append(", targetPreferredAuthSchemes=").append(targetPreferredAuthSchemes);
        builder.append(", proxyPreferredAuthSchemes=").append(proxyPreferredAuthSchemes);
        builder.append(", connectionRequestTimeout=").append(connectionRequestTimeout);
        builder.append(", connectTimeout=").append(connectTimeout);
        builder.append(", socketTimeout=").append(socketTimeout);
        builder.append("]");
        return builder.toString();
    }

    public static RequestConfig.Builder custom() {
        return new Builder();
    }

    public static RequestConfig.Builder copy(final RequestConfig config) {
        return new Builder()
            .setExpectContinueEnabled(config.isExpectContinueEnabled())
            .setProxy(config.getProxy())
            .setLocalAddress(config.getLocalAddress())
            .setStaleConnectionCheckEnabled(config.isStaleConnectionCheckEnabled())
            .setCookieSpec(config.getCookieSpec())
            .setRedirectsEnabled(config.isRedirectsEnabled())
            .setRelativeRedirectsAllowed(config.isRelativeRedirectsAllowed())
            .setCircularRedirectsAllowed(config.isCircularRedirectsAllowed())
            .setMaxRedirects(config.getMaxRedirects())
            .setAuthenticationEnabled(config.isAuthenticationEnabled())
            .setTargetPreferredAuthSchemes(config.getTargetPreferredAuthSchemes())
            .setProxyPreferredAuthSchemes(config.getProxyPreferredAuthSchemes())
            .setConnectionRequestTimeout(config.getConnectionRequestTimeout())
            .setConnectTimeout(config.getConnectTimeout())
            .setSocketTimeout(config.getSocketTimeout());
    }

    public static class Builder {

        private boolean expectContinueEnabled;
        private HttpHost proxy;
        private InetAddress localAddress;
        private boolean staleConnectionCheckEnabled;
        private String cookieSpec;
        private boolean redirectsEnabled;
        private boolean relativeRedirectsAllowed;
        private boolean circularRedirectsAllowed;
        private int maxRedirects;
        private boolean authenticationEnabled;
        private Collection<String> targetPreferredAuthSchemes;
        private Collection<String> proxyPreferredAuthSchemes;
        private int connectionRequestTimeout;
        private int connectTimeout;
        private int socketTimeout;

        Builder() {
            super();
            this.staleConnectionCheckEnabled = true;
            this.redirectsEnabled = true;
            this.maxRedirects = 50;
            this.relativeRedirectsAllowed = true;
            this.authenticationEnabled = true;
            this.connectionRequestTimeout = -1;
            this.connectTimeout = -1;
            this.socketTimeout = -1;
        }

        public Builder setExpectContinueEnabled(final boolean expectContinueEnabled) {
            this.expectContinueEnabled = expectContinueEnabled;
            return this;
        }

        public Builder setProxy(final HttpHost proxy) {
            this.proxy = proxy;
            return this;
        }

        public Builder setLocalAddress(final InetAddress localAddress) {
            this.localAddress = localAddress;
            return this;
        }

        public Builder setStaleConnectionCheckEnabled(final boolean staleConnectionCheckEnabled) {
            this.staleConnectionCheckEnabled = staleConnectionCheckEnabled;
            return this;
        }

        public Builder setCookieSpec(final String cookieSpec) {
            this.cookieSpec = cookieSpec;
            return this;
        }

        public Builder setRedirectsEnabled(final boolean redirectsEnabled) {
            this.redirectsEnabled = redirectsEnabled;
            return this;
        }

        public Builder setRelativeRedirectsAllowed(final boolean relativeRedirectsAllowed) {
            this.relativeRedirectsAllowed = relativeRedirectsAllowed;
            return this;
        }

        public Builder setCircularRedirectsAllowed(final boolean circularRedirectsAllowed) {
            this.circularRedirectsAllowed = circularRedirectsAllowed;
            return this;
        }

        public Builder setMaxRedirects(final int maxRedirects) {
            this.maxRedirects = maxRedirects;
            return this;
        }

        public Builder setAuthenticationEnabled(final boolean authenticationEnabled) {
            this.authenticationEnabled = authenticationEnabled;
            return this;
        }

        public Builder setTargetPreferredAuthSchemes(final Collection<String> targetPreferredAuthSchemes) {
            this.targetPreferredAuthSchemes = targetPreferredAuthSchemes;
            return this;
        }

        public Builder setProxyPreferredAuthSchemes(final Collection<String> proxyPreferredAuthSchemes) {
            this.proxyPreferredAuthSchemes = proxyPreferredAuthSchemes;
            return this;
        }

        public Builder setConnectionRequestTimeout(final int connectionRequestTimeout) {
            this.connectionRequestTimeout = connectionRequestTimeout;
            return this;
        }

        public Builder setConnectTimeout(final int connectTimeout) {
            this.connectTimeout = connectTimeout;
            return this;
        }

        public Builder setSocketTimeout(final int socketTimeout) {
            this.socketTimeout = socketTimeout;
            return this;
        }

        public RequestConfig build() {
            return new RequestConfig(
                    expectContinueEnabled,
                    proxy,
                    localAddress,
                    staleConnectionCheckEnabled,
                    cookieSpec,
                    redirectsEnabled,
                    relativeRedirectsAllowed,
                    circularRedirectsAllowed,
                    maxRedirects,
                    authenticationEnabled,
                    targetPreferredAuthSchemes,
                    proxyPreferredAuthSchemes,
                    connectionRequestTimeout,
                    connectTimeout,
                    socketTimeout);
        }

    }

}
