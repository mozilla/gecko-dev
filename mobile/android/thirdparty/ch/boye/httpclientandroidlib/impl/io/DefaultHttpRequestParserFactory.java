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

package ch.boye.httpclientandroidlib.impl.io;

import ch.boye.httpclientandroidlib.HttpRequest;
import ch.boye.httpclientandroidlib.HttpRequestFactory;
import ch.boye.httpclientandroidlib.annotation.Immutable;
import ch.boye.httpclientandroidlib.config.MessageConstraints;
import ch.boye.httpclientandroidlib.impl.DefaultHttpRequestFactory;
import ch.boye.httpclientandroidlib.io.HttpMessageParser;
import ch.boye.httpclientandroidlib.io.HttpMessageParserFactory;
import ch.boye.httpclientandroidlib.io.SessionInputBuffer;
import ch.boye.httpclientandroidlib.message.BasicLineParser;
import ch.boye.httpclientandroidlib.message.LineParser;

/**
 * Default factory for request message parsers.
 *
 * @since 4.3
 */
@Immutable
public class DefaultHttpRequestParserFactory implements HttpMessageParserFactory<HttpRequest> {

    public static final DefaultHttpRequestParserFactory INSTANCE = new DefaultHttpRequestParserFactory();

    private final LineParser lineParser;
    private final HttpRequestFactory requestFactory;

    public DefaultHttpRequestParserFactory(final LineParser lineParser,
            final HttpRequestFactory requestFactory) {
        super();
        this.lineParser = lineParser != null ? lineParser : BasicLineParser.INSTANCE;
        this.requestFactory = requestFactory != null ? requestFactory
                : DefaultHttpRequestFactory.INSTANCE;
    }

    public DefaultHttpRequestParserFactory() {
        this(null, null);
    }

    public HttpMessageParser<HttpRequest> create(final SessionInputBuffer buffer,
            final MessageConstraints constraints) {
        return new DefaultHttpRequestParser(buffer, lineParser, requestFactory, constraints);
    }

}
