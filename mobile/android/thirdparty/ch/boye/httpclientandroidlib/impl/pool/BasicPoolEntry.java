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
package ch.boye.httpclientandroidlib.impl.pool;

import java.io.IOException;

import ch.boye.httpclientandroidlib.HttpClientConnection;
import ch.boye.httpclientandroidlib.HttpHost;
import ch.boye.httpclientandroidlib.annotation.ThreadSafe;
import ch.boye.httpclientandroidlib.pool.PoolEntry;

/**
 * A very basic {@link PoolEntry} implementation that represents an entry
 * in a pool of blocking {@link HttpClientConnection}s identified by
 * an {@link HttpHost} instance.
 *
 * @see HttpHost
 * @since 4.2
 */
@ThreadSafe
public class BasicPoolEntry extends PoolEntry<HttpHost, HttpClientConnection> {

    public BasicPoolEntry(final String id, final HttpHost route, final HttpClientConnection conn) {
        super(id, route, conn);
    }

    @Override
    public void close() {
        try {
            this.getConnection().close();
        } catch (final IOException ignore) {
        }
    }

    @Override
    public boolean isClosed() {
        return !this.getConnection().isOpen();
    }

}
