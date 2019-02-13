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

package ch.boye.httpclientandroidlib.auth.params;

import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.params.HttpParams;
import ch.boye.httpclientandroidlib.protocol.HTTP;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * An adaptor for manipulating HTTP authentication parameters
 * in {@link HttpParams}.
 *
 * @since 4.0
 *
 * @deprecated (4.3) use {@link ch.boye.httpclientandroidlib.client.config.RequestConfig}
 *   and constructor parameters of
 *   {@link ch.boye.httpclientandroidlib.auth.AuthSchemeProvider}s.
 */
@Immutable
@Deprecated
public final class AuthParams {

    private AuthParams() {
        super();
    }

    /**
     * Obtains the charset for encoding
     * {@link ch.boye.httpclientandroidlib.auth.Credentials}.If not configured,
     * {@link HTTP#DEFAULT_PROTOCOL_CHARSET}is used instead.
     *
     * @return The charset
     */
    public static String getCredentialCharset(final HttpParams params) {
        Args.notNull(params, "HTTP parameters");
        String charset = (String) params.getParameter
            (AuthPNames.CREDENTIAL_CHARSET);
        if (charset == null) {
            charset = HTTP.DEF_PROTOCOL_CHARSET.name();
        }
        return charset;
    }


    /**
     * Sets the charset to be used when encoding
     * {@link ch.boye.httpclientandroidlib.auth.Credentials}.
     *
     * @param charset The charset
     */
    public static void setCredentialCharset(final HttpParams params, final String charset) {
        Args.notNull(params, "HTTP parameters");
        params.setParameter(AuthPNames.CREDENTIAL_CHARSET, charset);
    }

}
