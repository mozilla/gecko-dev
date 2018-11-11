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
package ch.boye.httpclientandroidlib.client.entity;

import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;

import java.io.IOException;
import java.io.InputStream;

/**
 * Lazy init InputStream wrapper.
 */
@NotThreadSafe
class LazyDecompressingInputStream extends InputStream {

    private final InputStream wrappedStream;

    private final DecompressingEntity decompressingEntity;

    private InputStream wrapperStream;

    public LazyDecompressingInputStream(
            final InputStream wrappedStream,
            final DecompressingEntity decompressingEntity) {
        this.wrappedStream = wrappedStream;
        this.decompressingEntity = decompressingEntity;
    }

    private void initWrapper() throws IOException {
        if (wrapperStream == null) {
            wrapperStream = decompressingEntity.decorate(wrappedStream);
        }
    }

    @Override
    public int read() throws IOException {
        initWrapper();
        return wrapperStream.read();
    }

    @Override
    public int read(final byte[] b) throws IOException {
        initWrapper();
        return wrapperStream.read(b);
    }

    @Override
    public int read(final byte[] b, final int off, final int len) throws IOException {
        initWrapper();
        return wrapperStream.read(b, off, len);
    }

    @Override
    public long skip(final long n) throws IOException {
        initWrapper();
        return wrapperStream.skip(n);
    }

    @Override
    public boolean markSupported() {
        return false;
    }

    @Override
    public int available() throws IOException {
        initWrapper();
        return wrapperStream.available();
    }

    @Override
    public void close() throws IOException {
        try {
            if (wrapperStream != null) {
                wrapperStream.close();
            }
        } finally {
            wrappedStream.close();
        }
    }

}
