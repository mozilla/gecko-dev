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

package ch.boye.httpclientandroidlib.protocol;

import java.io.IOException;

import ch.boye.httpclientandroidlib.HttpEntity;
import ch.boye.httpclientandroidlib.HttpException;
import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.HttpResponseInterceptor;
import ch.boye.httpclientandroidlib.HttpStatus;
import ch.boye.httpclientandroidlib.HttpVersion;
import ch.boye.httpclientandroidlib.ProtocolException;
import ch.boye.httpclientandroidlib.ProtocolVersion;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * ResponseContent is the most important interceptor for outgoing responses.
 * It is responsible for delimiting content length by adding
 * <code>Content-Length</code> or <code>Transfer-Content</code> headers based
 * on the properties of the enclosed entity and the protocol version.
 * This interceptor is required for correct functioning of server side protocol
 * processors.
 *
 * @since 4.0
 */
@Immutable
public class ResponseContent implements HttpResponseInterceptor {

    private final boolean overwrite;

    /**
     * Default constructor. The <code>Content-Length</code> or <code>Transfer-Encoding</code>
     * will cause the interceptor to throw {@link ProtocolException} if already present in the
     * response message.
     */
    public ResponseContent() {
        this(false);
    }

    /**
     * Constructor that can be used to fine-tune behavior of this interceptor.
     *
     * @param overwrite If set to <code>true</code> the <code>Content-Length</code> and
     * <code>Transfer-Encoding</code> headers will be created or updated if already present.
     * If set to <code>false</code> the <code>Content-Length</code> and
     * <code>Transfer-Encoding</code> headers will cause the interceptor to throw
     * {@link ProtocolException} if already present in the response message.
     *
     * @since 4.2
     */
     public ResponseContent(final boolean overwrite) {
         super();
         this.overwrite = overwrite;
    }

    /**
     * Processes the response (possibly updating or inserting) Content-Length and Transfer-Encoding headers.
     * @param response The HttpResponse to modify.
     * @param context Unused.
     * @throws ProtocolException If either the Content-Length or Transfer-Encoding headers are found.
     * @throws IllegalArgumentException If the response is null.
     */
    public void process(final HttpResponse response, final HttpContext context)
            throws HttpException, IOException {
        Args.notNull(response, "HTTP response");
        if (this.overwrite) {
            response.removeHeaders(HTTP.TRANSFER_ENCODING);
            response.removeHeaders(HTTP.CONTENT_LEN);
        } else {
            if (response.containsHeader(HTTP.TRANSFER_ENCODING)) {
                throw new ProtocolException("Transfer-encoding header already present");
            }
            if (response.containsHeader(HTTP.CONTENT_LEN)) {
                throw new ProtocolException("Content-Length header already present");
            }
        }
        final ProtocolVersion ver = response.getStatusLine().getProtocolVersion();
        final HttpEntity entity = response.getEntity();
        if (entity != null) {
            final long len = entity.getContentLength();
            if (entity.isChunked() && !ver.lessEquals(HttpVersion.HTTP_1_0)) {
                response.addHeader(HTTP.TRANSFER_ENCODING, HTTP.CHUNK_CODING);
            } else if (len >= 0) {
                response.addHeader(HTTP.CONTENT_LEN, Long.toString(entity.getContentLength()));
            }
            // Specify a content type if known
            if (entity.getContentType() != null && !response.containsHeader(
                    HTTP.CONTENT_TYPE )) {
                response.addHeader(entity.getContentType());
            }
            // Specify a content encoding if known
            if (entity.getContentEncoding() != null && !response.containsHeader(
                    HTTP.CONTENT_ENCODING)) {
                response.addHeader(entity.getContentEncoding());
            }
        } else {
            final int status = response.getStatusLine().getStatusCode();
            if (status != HttpStatus.SC_NO_CONTENT
                    && status != HttpStatus.SC_NOT_MODIFIED
                    && status != HttpStatus.SC_RESET_CONTENT) {
                response.addHeader(HTTP.CONTENT_LEN, "0");
            }
        }
    }

}
