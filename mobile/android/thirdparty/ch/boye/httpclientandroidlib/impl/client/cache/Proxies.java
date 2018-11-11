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

import java.lang.reflect.Proxy;

import ch.boye.httpclientandroidlib.HttpResponse;
import ch.boye.httpclientandroidlib.annotation.NotThreadSafe;
import ch.boye.httpclientandroidlib.client.methods.CloseableHttpResponse;
import ch.boye.httpclientandroidlib.util.Args;

/**
 * Proxies for HTTP message objects.
 *
 * @since 4.3
 */
@NotThreadSafe
class Proxies {

    public static CloseableHttpResponse enhanceResponse(final HttpResponse original) {
        Args.notNull(original, "HTTP response");
        if (original instanceof CloseableHttpResponse) {
            return (CloseableHttpResponse) original;
        } else {
            return (CloseableHttpResponse) Proxy.newProxyInstance(
                    ResponseProxyHandler.class.getClassLoader(),
                    new Class<?>[] { CloseableHttpResponse.class },
                    new ResponseProxyHandler(original));
        }
    }

}
