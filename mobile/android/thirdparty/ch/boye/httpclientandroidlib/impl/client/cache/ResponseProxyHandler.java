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

package ch.boye.httpclientandroidlib.impl.client.cache;

import java.io.Closeable;
import java.io.IOException;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;

/**
 * A proxy class that can enhance an arbitrary {@link HttpResponse} with
 * {@link Closeable#close()} method.
 *
 * @since 4.3
 */
@NotThreadSafe
class ResponseProxyHandler implements InvocationHandler {

    private static final Method CLOSE_METHOD;

    static {
        try {
            CLOSE_METHOD = Closeable.class.getMethod("close");
        } catch (final NoSuchMethodException ex) {
            throw new Error(ex);
        }
    }

    private final HttpResponse original;

    ResponseProxyHandler(final HttpResponse original) {
        super();
        this.original = original;
    }

    public void close() throws IOException {
        IOUtils.consume(original.getEntity());
    }

    public Object invoke(
            final Object proxy, final Method method, final Object[] args) throws Throwable {
        if (method.equals(CLOSE_METHOD)) {
            close();
            return null;
        } else {
            try {
                return method.invoke(this.original, args);
            } catch (final InvocationTargetException ex) {
                final Throwable cause = ex.getCause();
                if (cause != null) {
                    throw cause;
                } else {
                    throw ex;
                }
            }
        }
    }

}
