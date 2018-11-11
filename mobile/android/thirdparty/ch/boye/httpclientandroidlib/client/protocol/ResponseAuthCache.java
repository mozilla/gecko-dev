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

package ch.boye.httpclientandroidlib.client.protocol;

import java.io.IOException;

import ch.boye.httpclientandroidlib.androidextra.HttpClientAndroidLog;
/* LogFactory removed by HttpClient for Android script. */
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpResponseInterceptor;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.auth.AuthScheme;
import ch.boye.httpclientandroidlib.auth.AuthState;
import ch.boye.httpclientandroidlib.client.AuthCache;
import ch.boye.httpclientandroidlib.client.params.AuthPolicy;
import ch.boye.httpclientandroidlib.conn.scheme.Scheme;
import ch.boye.httpclientandroidlib.conn.scheme.SchemeRegistry;
import ch.boye.httpclientandroidlib.impl.client.BasicAuthCache;
import ch.boye.httpclientandroidlib.protocol.ExecutionContext;
import ch.boye.httpclientandroidlib.protocol.HttpContext;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * Response interceptor that adds successfully completed {@link AuthScheme}s
 * to the local {@link AuthCache} instance. Cached {@link AuthScheme}s can be
 * re-used when executing requests against known hosts, thus avoiding
 * additional authentication round-trips.
 *
 * @since 4.1
 *
 * @deprecated (4.2)  use {@link ch.boye.httpclientandroidlib.client.AuthenticationStrategy}
 */
@Immutable
@Deprecated
public class ResponseAuthCache implements HttpResponseInterceptor {

    public HttpClientAndroidLog log = new HttpClientAndroidLog(getClass());

    public ResponseAuthCache() {
        super();
    }

    public void process(final HttpResponse response, final HttpContext context)
            throws HttpException, IOException {
        Args.notNull(response, "HTTP request");
        Args.notNull(context, "HTTP context");
        AuthCache authCache = (AuthCache) context.getAttribute(ClientContext.AUTH_CACHE);

        HttpHost target = (HttpHost) context.getAttribute(ExecutionContext.HTTP_TARGET_HOST);
        final AuthState targetState = (AuthState) context.getAttribute(ClientContext.TARGET_AUTH_STATE);
        if (target != null && targetState != null) {
            if (this.log.isDebugEnabled()) {
                this.log.debug("Target auth state: " + targetState.getState());
            }
            if (isCachable(targetState)) {
                final SchemeRegistry schemeRegistry = (SchemeRegistry) context.getAttribute(
                        ClientContext.SCHEME_REGISTRY);
                if (target.getPort() < 0) {
                    final Scheme scheme = schemeRegistry.getScheme(target);
                    target = new HttpHost(target.getHostName(),
                            scheme.resolvePort(target.getPort()), target.getSchemeName());
                }
                if (authCache == null) {
                    authCache = new BasicAuthCache();
                    context.setAttribute(ClientContext.AUTH_CACHE, authCache);
                }
                switch (targetState.getState()) {
                case CHALLENGED:
                    cache(authCache, target, targetState.getAuthScheme());
                    break;
                case FAILURE:
                    uncache(authCache, target, targetState.getAuthScheme());
                }
            }
        }

        final HttpHost proxy = (HttpHost) context.getAttribute(ExecutionContext.HTTP_PROXY_HOST);
        final AuthState proxyState = (AuthState) context.getAttribute(ClientContext.PROXY_AUTH_STATE);
        if (proxy != null && proxyState != null) {
            if (this.log.isDebugEnabled()) {
                this.log.debug("Proxy auth state: " + proxyState.getState());
            }
            if (isCachable(proxyState)) {
                if (authCache == null) {
                    authCache = new BasicAuthCache();
                    context.setAttribute(ClientContext.AUTH_CACHE, authCache);
                }
                switch (proxyState.getState()) {
                case CHALLENGED:
                    cache(authCache, proxy, proxyState.getAuthScheme());
                    break;
                case FAILURE:
                    uncache(authCache, proxy, proxyState.getAuthScheme());
                }
            }
        }
    }

    private boolean isCachable(final AuthState authState) {
        final AuthScheme authScheme = authState.getAuthScheme();
        if (authScheme == null || !authScheme.isComplete()) {
            return false;
        }
        final String schemeName = authScheme.getSchemeName();
        return schemeName.equalsIgnoreCase(AuthPolicy.BASIC) ||
                schemeName.equalsIgnoreCase(AuthPolicy.DIGEST);
    }

    private void cache(final AuthCache authCache, final HttpHost host, final AuthScheme authScheme) {
        if (this.log.isDebugEnabled()) {
            this.log.debug("Caching '" + authScheme.getSchemeName() +
                    "' auth scheme for " + host);
        }
        authCache.put(host, authScheme);
    }

    private void uncache(final AuthCache authCache, final HttpHost host, final AuthScheme authScheme) {
        if (this.log.isDebugEnabled()) {
            this.log.debug("Removing from cache '" + authScheme.getSchemeName() +
                    "' auth scheme for " + host);
        }
        authCache.remove(host);
    }
}
